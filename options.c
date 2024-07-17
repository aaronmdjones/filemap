/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "filemap.h"

static void
fm_print_usage(void)
{
	(void) fprintf(stderr, "\n"
	    "  Usage: filemap -h\n"
	    "  Usage: filemap [-A | -D] [-O | -L | -C | -H | -N | -S | -F]\n"
	    "                 [-d -f -g -q -x -y] [[-o -l -s -t] | -r] <path>\n"
	    "\n"
	    "    -h / --help               Show this help message and exit\n"
	    "\n"
	    "    -A / --sort-ascending     Display extents in ascending order\n"
	    "    -D / --sort-descending    Display extents in descending order\n"
	    "\n"
	    "    -O / --order-offset       Order extents by physical offset\n"
	    "    -L / --order-length       Order extents by physical length\n"
	    "    -C / --order-count        Order extents by number of extents\n"
	    "    -H / --order-links        Order extents by number of hardlinks\n"
	    "    -N / --order-inum         Order extents by inode number\n"
	    "    -S / --order-filesize     Order extents by file size\n"
	    "    -F / --order-filename     Order extents by file name\n"
	    "\n"
	    "    -d / --scan-directories   Scan the extents that belong to\n"
	    "                              directories as well as regular files\n"
	    "    -f / --fragmented-only    Print fragmented files only\n"
	    "    -g / --print-gaps         Print the gaps between extents\n"
	    "                              Needs --sort-ascending --order-offset\n"
	    "                              Incompatible with --fragmented-only\n"
	    "    -q / --quiet              Don't print the action being performed\n"
	    "    -x / --skip-preamble      Skip the informational message lines\n"
	    "                              printed before the table of extents\n"
	    "    -y / --sync-files         Invoke fsync(2) on everything being\n"
	    "                              scanned before scanning it\n"
	    "\n"
	    "    -o / --readable-offsets   Print human-readable extent offsets\n"
	    "    -l / --readable-lengths   Print human-readable extent lengths\n"
	    "    -s / --readable-sizes     Print human-readable file sizes\n"
	    "    -t / --readable-gaps      Print human-readable extent gaps\n"
	    "    -r / --readable-all       Short-hand for the above 4 options;\n"
	    "                              implies '-o -l -s -t'\n"
	    "\n"
	    "  Notes:\n"
	    "\n"
	    "    The default options are '--sort-ascending --order-offset', to\n"
	    "    display the list of extents in the order that they appear in the\n"
	    "    volume.\n"
	    "\n"
	    "    For option '--order-filename', only the alphabetically-first\n"
	    "    file name for each inode (in the case of hardlinks) is considered\n"
	    "    when determining the order. The file names shown next to each\n"
	    "    extent in the results will also be sorted alphabetically.\n"
	    "\n"
	    "    For the most comprehensive results, ensure <path> is the root of\n"
	    "    a filesystem that supports extents, and that you have permission\n"
	    "    to open (read-only) every file in that filesystem. You should also\n"
	    "    give the -d and -y options to map the extents that are assigned to\n"
	    "    directories and to ensure that everything being mapped has already\n"
	    "    been written out to the underlying storage.\n"
	    "\n"
	);

	(void) fflush(stderr);
}

enum fm_optparse_result FM_NONNULL(2) FM_WARN_UNUSED
fm_parse_options(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{             "help", 0, NULL, 'h' },
		{   "sort-ascending", 0, NULL, 'A' },
		{  "sort-descending", 0, NULL, 'D' },
		{     "order-offset", 0, NULL, 'O' },
		{     "order-length", 0, NULL, 'L' },
		{      "order-count", 0, NULL, 'C' },
		{      "order-links", 0, NULL, 'H' },
		{       "order-inum", 0, NULL, 'N' },
		{   "order-filesize", 0, NULL, 'S' },
		{   "order-filename", 0, NULL, 'F' },
		{ "scan-directories", 0, NULL, 'd' },
		{  "fragmented-only", 0, NULL, 'f' },
		{       "print-gaps", 0, NULL, 'g' },
		{            "quiet", 0, NULL, 'q' },
		{    "skip-preamble", 0, NULL, 'x' },
		{       "sync-files", 0, NULL, 'y' },
		{ "readable-offsets", 0, NULL, 'o' },
		{ "readable-lengths", 0, NULL, 'l' },
		{   "readable-sizes", 0, NULL, 's' },
		{    "readable-gaps", 0, NULL, 't' },
		{     "readable-all", 0, NULL, 'r' },
		{               NULL, 0, NULL,  0  },
	};

	static const char shortopts[] = "hADOLCHNSFdfgqxyolstr";

	argvzero = argv[0];

	for (;;)
	{
		const int opt = getopt_long(argc, argv, shortopts, longopts, NULL);

		if (opt == -1)
			break;

		switch (opt)
		{
			case 'h':
				(void) fm_print_usage();
				return FM_OPTPARSE_EXIT_SUCCESS;

			case 'A':
				fm_sort_direction = FM_SORTDIR_ASCENDING;
				break;

			case 'D':
				fm_sort_direction = FM_SORTDIR_DESCENDING;
				break;

			case 'O':
				fm_sort_method = FM_SORTMETH_EXTENT_OFFSET;
				break;

			case 'L':
				fm_sort_method = FM_SORTMETH_EXTENT_LENGTH;
				break;

			case 'C':
				fm_sort_method = FM_SORTMETH_INODE_EXTENT_COUNT;
				break;

			case 'H':
				fm_sort_method = FM_SORTMETH_INODE_LINK_COUNT;
				break;

			case 'N':
				fm_sort_method = FM_SORTMETH_INODE_NUMBER;
				break;

			case 'S':
				fm_sort_method = FM_SORTMETH_FILESIZE;
				break;

			case 'F':
				fm_sort_method = FM_SORTMETH_FILENAME;
				break;

			case 'd':
				fm_scan_directories = true;
				break;

			case 'f':
				fm_fragmented_only = true;
				break;

			case 'g':
				fm_print_gaps = true;
				break;

			case 'q':
				fm_run_quietly = true;
				break;

			case 'x':
				fm_skip_preamble = true;
				break;

			case 'y':
				fm_sync_files = true;
				break;

			case 'o':
				fm_readable_offsets = true;
				break;

			case 'l':
				fm_readable_lengths = true;
				break;

			case 's':
				fm_readable_sizes = true;
				break;

			case 't':
				fm_readable_gaps = true;
				break;

			case 'r':
				fm_readable_offsets = true;
				fm_readable_lengths = true;
				fm_readable_sizes = true;
				fm_readable_gaps = true;
				break;

			default:
				(void) fm_print_usage();
				return FM_OPTPARSE_EXIT_FAILURE;
		}
	}

	if (fm_print_gaps && fm_sort_direction != FM_SORTDIR_ASCENDING)
	{
		(void) fm_print_usage();
		return FM_OPTPARSE_EXIT_FAILURE;
	}
	if (fm_print_gaps && fm_sort_method != FM_SORTMETH_EXTENT_OFFSET)
	{
		(void) fm_print_usage();
		return FM_OPTPARSE_EXIT_FAILURE;
	}
	if (fm_print_gaps && fm_fragmented_only)
	{
		(void) fm_print_usage();
		return FM_OPTPARSE_EXIT_FAILURE;
	}
	if (optind >= argc)
	{
		(void) fm_print_usage();
		return FM_OPTPARSE_EXIT_FAILURE;
	}

	return FM_OPTPARSE_CONTINUE;
}
