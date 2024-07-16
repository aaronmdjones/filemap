/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "filemap.h"

bool FM_NONNULL(2, 3) FM_WARN_UNUSED
fm_scan_directory(const int fd, const struct stat *const restrict sb, const char *const restrict abspath)
{
	DIR *dp;

	if (! fm_run_quietly)
		(void) fm_print_message("%s: scanning %s ...", argvzero, abspath);

	if (fm_sync_files && fsync(fd) < 0)
	{
		(void) fm_print_message("%s: while scanning '%s': fsync(2): %s\n",
		                        argvzero, abspath, strerror(errno));
		return false;
	}
	if ((dp = fdopendir(fd)) == NULL)
	{
		(void) fm_print_message("%s: while scanning '%s': fdopendir(3): %s\n",
		                        argvzero, abspath, strerror(errno));
		return false;
	}

	for (;;)
	{
		char entpath[PATH_MAX];
		struct dirent *ede;
		struct stat esb;
		int efd;

		if (! fm_run_quietly)
			(void) fm_print_message("%s: walking %s ...", argvzero, abspath);

		errno = 0;

		if ((ede = readdir(dp)) == NULL)
		{
			if (errno == 0)
				// End of directory entries
				break;

			(void) fm_print_message("%s: while walking '%s': readdir(3): %s\n",
			                        argvzero, abspath, strerror(errno));
			return false;
		}

		if (strcmp(ede->d_name, ".") == 0 || strcmp(ede->d_name, "..") == 0)
			// Avoid infinite loops
			continue;

		(void) memset(entpath, 0x00, sizeof entpath);
		(void) memset(&esb, 0x00, sizeof esb);

		// Prepend a slash to the directory entry name only if the directory is not /
		(void) snprintf(entpath, sizeof entpath, "%s%s%s",
		                abspath, ((strcmp(abspath, "/") != 0) ? "/" : ""), ede->d_name);

		if (fstatat(fd, ede->d_name, &esb, AT_SYMLINK_NOFOLLOW) < 0)
		{
			(void) fm_print_message("%s: while scanning '%s': fstatat(2): %s\n",
			                        argvzero, entpath, strerror(errno));
			return false;
		}

		if (esb.st_dev != sb->st_dev)
			// Not on the same filesystem
			continue;

		if (! (((esb.st_mode & S_IFMT) == S_IFDIR) || ((esb.st_mode & S_IFMT) == S_IFREG)))
			// Not a file or directory
			continue;

		(void) memset(&esb, 0x00, sizeof esb);

		if ((efd = openat(fd, ede->d_name, O_NOCTTY | O_RDONLY | O_NOFOLLOW, 0)) < 0)
		{
			(void) fm_print_message("%s: while scanning '%s': openat(2): %s\n",
			                        argvzero, entpath, strerror(errno));
			return false;
		}
		if (fstat(efd, &esb) < 0)
		{
			(void) fm_print_message("%s: while scanning '%s': fstat(2): %s\n",
			                        argvzero, entpath, strerror(errno));
			return false;
		}
		if (esb.st_dev != sb->st_dev)
		{
			// Not on the same filesystem
			(void) close(efd);
			continue;
		}
		if ((esb.st_mode & S_IFMT) == S_IFDIR)
		{
			if (! fm_scan_directory(efd, &esb, entpath))
				// This function prints messages on error
				return false;

			// The function called above will close the fd
			continue;
		}
		if ((esb.st_mode & S_IFMT) == S_IFREG)
		{
			if (! fm_scan_extents(efd, &esb, entpath))
				// This function prints messages on error
				return false;

			// The function called above will close the fd
			continue;
		}

		// This directory entry wasn't a file or directory; close the fd
		(void) close(efd);
	}

	if (fm_scan_directories)
	{
		if (! fm_scan_extents(fd, sb, abspath))
			// This function prints messages on error
			return false;
	}

	// This will also close the fd
	(void) closedir(dp);

	return true;
}
