/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/fiemap.h>

#include "filemap.h"

static const char * FM_RETURNS_NONNULL
fm_readable_size(const enum fm_readable_which which, const uint64_t insize)
{
	static const size_t result_buflen = 128U;
	static char result_offset[128U];
	static char result_length[128U];
	static char result_size[128U];
	bool do_readable = false;
	char *result = NULL;

	switch (which)
	{
		case FM_READABLE_OFFSET:
			do_readable = fm_readable_offsets;
			result = result_offset;
			break;

		case FM_READABLE_LENGTH:
			do_readable = fm_readable_lengths;
			result = result_length;
			break;

		case FM_READABLE_SIZE:
			do_readable = fm_readable_sizes;
			result = result_size;
			break;
	}

	(void) memset(result, 0x00, result_buflen);

	if (do_readable)
	{
		static const char *suffixes[] = { "  B", "KiB", "MiB", "GiB", "TiB", "PiB" };
		long double insized = (long double) insize;
		unsigned int suffidx = 0U;

		while (insized >= 1024 && suffidx < 6U)
		{
			insized /= 1024;
			suffidx++;
		}

		(void) snprintf(result, result_buflen, "%.2Lf %s", insized, suffixes[suffidx]);
	}
	else
		(void) snprintf(result, result_buflen, "%" PRIu64, insize);

	return result;
}

static const char * FM_NONNULL(1) FM_RETURNS_NONNULL
fm_build_inode_flags(const struct fm_inode *const restrict inode)
{
	static char result[16U];

	(void) memset(result, 0x00, sizeof result);

	if (inode->flags & FM_IFLAGS_UNALIGNED)
		// Data is not aligned
		(void) strcat(result, "A");

	if ((inode->sb.st_mode & S_IFMT) == S_IFDIR)
		// This inode is a directory
		(void) strcat(result, "D");

	if (inode->flags & FM_IFLAGS_FRAGMENTED || inode->extcount != 1U)
		// Data is not contiguous
		(void) strcat(result, "F");

	if (inode->namecount > 1U)
		// This inode has multiple filenames (hardlinks)
		(void) strcat(result, "L");

	if (inode->extcount > 1U)
		// Data is made up of multiple extents
		(void) strcat(result, "M");

	if (inode->flags & FM_IFLAGS_UNORDERED)
		// Data is not in order
		(void) strcat(result, "U");

	return result;
}

static const char * FM_NONNULL(1) FM_RETURNS_NONNULL
fm_build_extent_flags(const struct fm_extent *const restrict extent)
{
	static char result[16U];

	(void) memset(result, 0x00, sizeof result);

	if (extent->flags & FIEMAP_EXTENT_NOT_ALIGNED)
		// Extent offset and/or length is not aligned (not a multiple of the filesystem block size)
		(void) strcat(result, "A");

	if (extent->inode->extcount > 1U && extent->pos != extent->inode->extcount)
		// Data is made up of multiple extents; this is not the last; data continues after this
		(void) strcat(result, "C");

	if (extent->flags & FIEMAP_EXTENT_DELALLOC)
		/* Delayed allocation; the block allocator is waiting for more data
		 * and/or looking for a suitable space to put the data it has
		 */
		(void) strcat(result, "D");

	if (extent->flags & FIEMAP_EXTENT_LAST)
		// This is the last extent (whether the data is made up of multiple extents or not)
		(void) strcat(result, "E");

	if (extent->flags & FIEMAP_EXTENT_DATA_INLINE)
		// Extent is located within a metadata block; inline allocation
		(void) strcat(result, "I");

	if (extent->flags & FIEMAP_EXTENT_MERGED)
		/* Filesystem does not support extents or this file is not using them;
		 * the kernel has merged contiguous filesystem data blocks into a
		 * pseudo extent for us instead
		 */
		(void) strcat(result, "M");

	if (extent->flags & FIEMAP_EXTENT_DATA_TAIL)
		// Extent contains data from multiple files
		(void) strcat(result, "T");

	if (extent->flags & FIEMAP_EXTENT_UNKNOWN)
		// No storage has been allocated for this extent yet
		(void) strcat(result, "U");

	if (extent->flags & FIEMAP_EXTENT_UNWRITTEN)
		/* Extent allocated but not initialised; reading from a file descriptor
		 * will return zeroes, but reading this extent directly from the volume
		 * may return different, possibly nonsensical data
		 */
		(void) strcat(result, "W");

	if (extent->flags & FIEMAP_EXTENT_ENCODED)
		/* This extent contains data that is encoded somehow (compressed, encrypted, ...);
		 * reading from a file descriptor will work normally, but reading this extent
		 * directly from the volume will return different data
		 */
		(void) strcat(result, "X");

	return result;
}

void FM_NONNULL(1) FM_PRINTF(1, 2)
fm_print_message(const char *const restrict fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);

	if (isatty(STDERR_FILENO) == 1)
	{
		(void) fprintf(stderr, "\033[2K\r");
		(void) vfprintf(stderr, fmt, argp);
		(void) fflush(stderr);
	}

	va_end(argp);
}

void
fm_print_results(void)
{
	const struct fm_name *fname;
	uint64_t fragged_extents = 0U;
	uint64_t fragged_inodes = 0U;
	struct fm_extent *extent;
	struct fm_extent *etmp;
	struct fm_inode *inode;
	struct fm_inode *itmp;

	(void) fm_print_message("");

	if (! fm_extent_count)
		return;

	HASH_ITER(hh, fm_inodes, inode, itmp)
	{
		if (! (inode->flags & FM_IFLAGS_FRAGMENTED))
			continue;

		fragged_extents += inode->extcount;
		fragged_inodes++;
	}

	if (fm_skip_preamble)
		goto results;

	const long double inofragpcnt = 100.0 * (((long double) fragged_inodes) / ((long double) fm_inode_count));
	const long double extfragratio = (((long double) fragged_extents) / ((long double) fragged_inodes));

	if (! (fm_fragmented_only && ! fragged_inodes))
	{
		// Only print information about interpreting upcoming extents if we are going to print any extents

		if (fm_readable_offsets)
			(void) printf("Extent offsets are in ....... : human-readable units\n");
		else if (fm_integral_blksz)
			(void) printf("Extent offsets are in ....... : multiples of filesystem blocks "
			              "(%" PRIu64 " bytes)\n", fm_blksz);
		else
			(void) printf("Extent offsets are in ....... : bytes\n");

		if (fm_readable_lengths)
			(void) printf("Extent lengths are in ....... : human-readable units\n");
		else if (fm_integral_blksz)
			(void) printf("Extent lengths are in ....... : multiples of filesystem blocks "
			              "(%" PRIu64 " bytes)\n", fm_blksz);
		else
			(void) printf("Extent lengths are in ....... : bytes\n");

		if (fm_readable_sizes)
			(void) printf("File sizes are in ........... : human-readable units\n");
		else
			(void) printf("File sizes are in ........... : bytes\n");
	}

	if (fm_scan_directories)
		(void) printf("Mapped ...................... : %" PRIu64 " files & %" PRIu64 " dirs (%" PRIu64
		              " inodes) consisting of %" PRIu64 " extents\n", fm_file_count, fm_dir_count,
		              fm_inode_count, fm_extent_count);
	else
		(void) printf("Mapped ...................... : %" PRIu64 " files (%" PRIu64 " inodes) consisting "
		              "of %" PRIu64 " extents\n", fm_file_count, fm_inode_count, fm_extent_count);

	if (fragged_inodes)
		(void) printf("Fragmented inodes ........... : %" PRIu64 "/%" PRIu64 " (%.2Lf%%); average %.2Lf "
		              "extents per fragmented inode\n", fragged_inodes, fm_inode_count, inofragpcnt,
		              extfragratio);

	if (fm_fragmented_only)
	{
		const char *const fwhich = ((fm_scan_directories) ? "files & dirs" : "files");

		(void) printf("\n");

		if (fragged_inodes)
			(void) printf("Requested to show only fragmented %s\n", fwhich);
		else
			(void) printf("Requested to show only fragmented %s; however, there are none\n", fwhich);
	}

results:

	if (fm_fragmented_only && ! fragged_inodes)
		return;

	(void) printf("\n");

	(void) printf("%20s %20s %12s %12s %12s %12s %20s    %s\n", "Extent Offset", "Extent Length",
	              "Extent Count", "Extent Flags", "Inode Number", "Inode Flags", "File Size", "File Name(s)");

	(void) printf("-------------------- -------------------- ------------ ------------ "
	              "------------ ------------ --------------------    ------------\n\n");

	HASH_ITER(hh, fm_extents, extent, etmp)
	{
		if (fm_fragmented_only && ! (extent->inode->flags & FM_IFLAGS_FRAGMENTED))
			continue;

		DL_FOREACH(extent->inode->names, fname)
		{
			if (fname == extent->inode->names)
			{
				const uint64_t extoff = ((fm_integral_blksz && ! fm_readable_offsets) ? \
				                        (extent->off / fm_blksz) : extent->off);
				const uint64_t extlen = ((fm_integral_blksz && ! fm_readable_lengths) ? \
				                        (extent->len / fm_blksz) : extent->len);
				const char *const inoflags = fm_build_inode_flags(extent->inode);
				const char *const extflags = fm_build_extent_flags(extent);
				const uint64_t fsize = (uint64_t) extent->inode->sb.st_size;
				char extpos[128U];

				(void) memset(extpos, 0x00, sizeof extpos);
				(void) snprintf(extpos, sizeof extpos, "%" PRIu64 "/%" PRIu64,
				                extent->pos, extent->inode->extcount);

				// Print full details for the first file name pointing to this inode
				(void) printf("%20s %20s %12s %12s %12" PRIu64 " %12s %20s    %s\n",
				              fm_readable_size(FM_READABLE_OFFSET, extoff),
				              fm_readable_size(FM_READABLE_LENGTH, extlen),
				              extpos, extflags, extent->inode->inum, inoflags,
				              fm_readable_size(FM_READABLE_SIZE, fsize),
				              extent->inode->names->name);
			}
			else if (! (extent->inode->flags & FM_IFLAGS_PRINTED))
			{
				/* Print only the file name for other file names pointing to this inode,
				 * but only if we have not yet done so for this inode already
				 */
				(void) printf("%20s %20s %12s %12s %12s %12s %20s    %s\n",
				              "----", "----", "----", "----", "----", "----", "----",
				              fname->name);
			}
			else
			{
				// We have already printed other file names for this inode, skip doing so
				(void) printf("%20s %20s %12s %12s %12s %12s %20s    %s\n",
				              "++++", "++++", "++++", "++++", "++++", "++++", "++++",
				              "++++");
				break;
			}
		}

		extent->inode->flags |= FM_IFLAGS_PRINTED;

		(void) fflush(stdout);
	}
}
