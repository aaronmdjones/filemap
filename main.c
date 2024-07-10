/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "filemap.h"

// Global variables (initialised to defaults, overridden by command-line options)
enum fm_sort_direction fm_sort_direction = FM_SORTDIR_ASCENDING;
enum fm_sort_method fm_sort_method = FM_SORTMETH_EXTENT_OFFSET;
bool fm_scan_directories = false;
bool fm_fragmented_only = false;
bool fm_skip_preamble = false;
bool fm_sync_files = false;
bool fm_readable_offsets = false;
bool fm_readable_lengths = false;
bool fm_readable_sizes = false;

// Global data structures
struct fm_extent *fm_extents = NULL;
struct fm_inode *fm_inodes = NULL;

// For statistics
bool fm_integral_blksz = true;
uint64_t fm_extent_count = 0U;
uint64_t fm_inode_count = 0U;
uint64_t fm_file_count = 0U;
uint64_t fm_dir_count = 0U;

// Miscellaneous (initialised in main())
const char *argvzero;
uint64_t fm_blksz;

int FM_NONNULL(2)
main(int argc, char *argv[])
{
	struct stat sb;
	int fd;

	(void) memset(&sb, 0x00, sizeof sb);

	switch (fm_parse_options(argc, argv))
	{
		case FM_OPTPARSE_EXIT_SUCCESS:
			return EXIT_SUCCESS;

		case FM_OPTPARSE_EXIT_FAILURE:
			return EXIT_FAILURE;

		case FM_OPTPARSE_CONTINUE:
			break;
	}
	if ((fd = open(argv[optind], O_NOCTTY | O_RDONLY | O_NOFOLLOW, 0)) < 0)
	{
		(void) fprintf(stderr, "%s: while scanning '%s': open(2): %s\n",
		                       argvzero, argv[optind], strerror(errno));
		(void) fflush(stderr);
		return EXIT_FAILURE;
	}
	if (fstat(fd, &sb) < 0)
	{
		(void) fprintf(stderr, "%s: while scanning '%s': fstat(2): %s\n",
		                       argvzero, argv[optind], strerror(errno));
		(void) fflush(stderr);
		return EXIT_FAILURE;
	}

	fm_blksz = (uint64_t) sb.st_blksize;

	if ((sb.st_mode & S_IFMT) == S_IFDIR)
	{
		if (! fm_scan_directory(fd, &sb, argv[optind]))
			// This function prints messages on error
			return EXIT_FAILURE;
	}
	else if ((sb.st_mode & S_IFMT) == S_IFREG)
	{
		if (! fm_scan_extents(fd, &sb, argv[optind]))
			// This function prints messages on error
			return EXIT_FAILURE;
	}
	else
	{
		(void) fprintf(stderr, "%s: while scanning '%s': not a file or directory\n",
		                       argvzero, argv[optind]);
		(void) fflush(stderr);
		return EXIT_FAILURE;
	}

	HASH_SORT(fm_extents, fm_sortby_extent_cb);

	(void) fm_print_results();

	return EXIT_SUCCESS;
}
