/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <linux/fiemap.h>
#include <linux/fs.h>

#include "filemap.h"

/* For FS_IOC_FIEMAP flexible array member accounting and reuse
 * Helps to avoid unnecessary memory allocation for each and every file queried
 */
static uint32_t fmh_extent_count = 0U;
static size_t fmh_extent_size = 0U;
static struct fiemap *fm = NULL;

bool FM_NONNULL(2, 3) FM_WARN_UNUSED
fm_scan_extents(const int fd, const struct stat *const restrict sb, const char *const restrict abspath)
{
	const uint64_t inum = (uint64_t) sb->st_ino;
	struct fm_inode *fi = NULL;

	if (! fm_run_quietly)
		(void) fm_print_message("%s: mapping %s ...", argvzero, abspath);

	HASH_FIND(hh, fm_inodes, &inum, sizeof inum, fi);

	if (fi == NULL)
	{
		struct fiemap fmh = {
			.fm_start          = 0U,
			.fm_length         = FIEMAP_MAX_OFFSET,
			.fm_flags          = ((fm_sync_files) ? FIEMAP_FLAG_SYNC : 0U),
			.fm_mapped_extents = 0U,
			.fm_extent_count   = 0U,
		};

		if (ioctl(fd, FS_IOC_FIEMAP, &fmh) < 0)
		{
			(void) fm_print_message("%s: while scanning '%s': ioctl(2) FS_IOC_FIEMAP: %s\n",
			                        argvzero, abspath, strerror(errno));
			return false;
		}
		if (fmh_extent_count <= fmh.fm_mapped_extents)
		{
			fmh_extent_count = (fmh.fm_mapped_extents + ((fmh.fm_mapped_extents + 256U) % 256U));
			fmh_extent_size = ((sizeof *fm) + (fmh_extent_count * sizeof(struct fiemap_extent)));

			if (! (fm = realloc(fm, fmh_extent_size)))
			{
				(void) fm_print_message("%s: while scanning '%s': realloc(3): %s\n",
				                        argvzero, abspath, strerror(errno));
				return false;
			}
		}

		(void) memset(fm, 0x00, fmh_extent_size);
		(void) memcpy(fm, &fmh, sizeof fmh);

		fm->fm_extent_count = fmh_extent_count;

		if (ioctl(fd, FS_IOC_FIEMAP, fm) < 0)
		{
			(void) fm_print_message("%s: while scanning '%s': ioctl(2) FS_IOC_FIEMAP: %s\n",
			                        argvzero, abspath, strerror(errno));
			return false;
		}
		if (fm->fm_mapped_extents == fmh_extent_count)
		{
			(void) fm_print_message("%s: while scanning '%s': truncated extents returned; "
			                        "file being written to?", argvzero, abspath);
			return false;
		}
		if ((fi = calloc(1U, sizeof *fi)) == NULL)
		{
			(void) fm_print_message("%s: while scanning '%s': calloc(3): %s\n",
			                        argvzero, abspath, strerror(errno));
			return false;
		}
		for (uint64_t i = 0U; i < fm->fm_mapped_extents; i++)
		{
			const uint64_t this_extoff = fm->fm_extents[i].fe_physical;
			const uint64_t this_extlen = fm->fm_extents[i].fe_length;
			const uint32_t this_extflg = fm->fm_extents[i].fe_flags;
			const uint64_t this_extpos = (i + 1U);
			struct fm_extent *fe;

			HASH_FIND(hh, fm_extents, &this_extoff, sizeof this_extoff, fe);

			if (fe != NULL)
			{
				(void) fm_print_message("%s: while scanning '%s': cannot handle files "
				                        "with shared extents\n", argvzero, abspath);
				return false;
			}
			if ((fe = calloc(1U, sizeof *fe)) == NULL)
			{
				(void) fm_print_message("%s: while scanning '%s': calloc(3): %s\n",
				                        argvzero, abspath, strerror(errno));
				return false;
			}
			if (this_extpos == fm->fm_mapped_extents && ! (this_extflg & FIEMAP_EXTENT_LAST))
			{
				(void) fm_print_message("%s: while scanning '%s': truncated extents returned; "
				                        "file being written to?", argvzero, abspath);
				return false;
			}

			fe->off   = this_extoff;
			fe->flags = this_extflg;
			fe->pos   = this_extpos;
			fe->len   = this_extlen;
			fe->inode = fi;

			if (i > 0U)
			{
				const uint64_t j = (i - 1U);
				const uint64_t prev_extoff = fm->fm_extents[j].fe_physical;
				const uint64_t prev_extlen = fm->fm_extents[j].fe_length;

				if (this_extoff > (prev_extoff + prev_extlen))
					fi->flags |= FM_IFLAGS_FRAGMENTED;

				if (this_extoff < prev_extoff)
					fi->flags |= FM_IFLAGS_FRAGMENTED | FM_IFLAGS_UNORDERED;
			}
			if ((this_extoff % fm_blksz) != 0U || (this_extlen % fm_blksz) != 0U)
			{
				fi->flags |= FM_IFLAGS_UNALIGNED;
				fm_integral_blksz = false;
			}

			HASH_ADD(hh, fm_extents, off, sizeof fe->off, fe);

			fi->extcount++;
		}

		(void) memcpy(&fi->sb, sb, sizeof fi->sb);

		fi->inum = inum;

		HASH_ADD(hh, fm_inodes, inum, sizeof fi->inum, fi);

		fm_extent_count += fi->extcount;
		fm_inode_count++;
	}

	struct fm_name *const fn = calloc(1U, sizeof *fn);

	if (fn == NULL)
	{
		(void) fm_print_message("%s: while scanning '%s': calloc(3): %s\n",
		                        argvzero, abspath, strerror(errno));
		return false;
	}

	if ((sb->st_mode & S_IFMT) == S_IFDIR)
		fm_dir_count++;
	else
		fm_file_count++;

	// Append a slash to the end of directory names, but only if the directory is not /
	(void) snprintf(fn->name, sizeof fn->name, "%s%s", abspath,
	                (((sb->st_mode & S_IFMT) == S_IFDIR && strcmp(abspath, "/") != 0) ? "/" : ""));

	fi->namecount++;
	fn->inode = fi;

	DL_APPEND(fi->names, fn);
	DL_SORT(fi->names, fm_sortby_filename_cb);

	(void) close(fd);

	return true;
}
