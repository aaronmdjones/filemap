/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023-2024 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#ifndef INC_FILEMAP_H
#define INC_FILEMAP_H 1

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uthash.h"
#include "utlist.h"

#define FM_NONNULL(...)                 __attribute__((__nonnull__(__VA_ARGS__)))
#define FM_PRINTF(...)                  __attribute__((__format__(printf, __VA_ARGS__)))
#define FM_RETURNS_NONNULL              __attribute__((__returns_nonnull__))
#define FM_WARN_UNUSED                  __attribute__((__warn_unused_result__))

#define FM_IFLAGS_NONE                  0x00U
#define FM_IFLAGS_FRAGMENTED            0x01U
#define FM_IFLAGS_UNORDERED             0x02U
#define FM_IFLAGS_UNALIGNED             0x04U
#define FM_IFLAGS_PRINTED               0x08U

enum fm_sort_direction
{
	FM_SORTDIR_ASCENDING            = 1,
	FM_SORTDIR_DESCENDING           = 2,
};

enum fm_sort_method
{
	FM_SORTMETH_EXTENT_OFFSET       = 1,
	FM_SORTMETH_EXTENT_LENGTH       = 2,
	FM_SORTMETH_INODE_EXTENT_COUNT  = 3,
	FM_SORTMETH_INODE_LINK_COUNT    = 4,
	FM_SORTMETH_INODE_NUMBER        = 5,
	FM_SORTMETH_FILESIZE            = 6,
	FM_SORTMETH_FILENAME            = 7,
};

enum fm_optparse_result
{
	FM_OPTPARSE_EXIT_SUCCESS        = 1,
	FM_OPTPARSE_EXIT_FAILURE        = 2,
	FM_OPTPARSE_CONTINUE            = 3,
};

enum fm_readable_which
{
	FM_READABLE_OFFSET              = 1,
	FM_READABLE_LENGTH              = 2,
	FM_READABLE_SIZE                = 3,
	FM_READABLE_GAP                 = 4,
};

struct fm_extent;
struct fm_inode;
struct fm_name;

struct fm_extent
{
	UT_hash_handle      hh;             // For entry into global struct fm_extent *fm_extents
	uint64_t            off;            // Physical offset of extent in volume (in bytes) (hash key)

	struct fm_inode *   inode;          // Which inode this extent belongs to; points to struct below
	uint64_t            len;            // Length of extent (in bytes)
	uint64_t            pos;            // The position of this extent in the inode's data
	uint32_t            flags;          // Extent flags (from the kernel)
};

struct fm_inode
{
	UT_hash_handle      hh;             // For entry into global struct fm_inode *fm_inodes
	uint64_t            inum;           // Inode number (hash key)

	struct stat         sb;             // Inode information (owner, mode, size, etc)
	struct fm_name *    names;          // Linked list of structs below
	uint64_t            extcount;       // Number of data extents in this inode
	uint64_t            namecount;      // Number of filenames that refer to this inode (hardlinks)
	uint32_t            flags;          // Bitfield of FI_FLAGS_*
};

struct fm_name
{
	struct fm_name *    prev;           // For entry into this->inode->names
	struct fm_name *    next;           // For entry into this->inode->names

	struct fm_inode *   inode;          // Which inode this filename points to; points to struct above
	char                name[PATH_MAX]; // The file name
};

// Global variables (initialised to defaults, overridden by command-line options)
// Located in main.c
extern enum fm_sort_direction fm_sort_direction;
extern enum fm_sort_method fm_sort_method;
extern bool fm_scan_directories;
extern bool fm_fragmented_only;
extern bool fm_print_gaps;
extern bool fm_run_quietly;
extern bool fm_skip_preamble;
extern bool fm_sync_files;
extern bool fm_readable_offsets;
extern bool fm_readable_lengths;
extern bool fm_readable_sizes;
extern bool fm_readable_gaps;

// Global data structures
// Located in main.c
extern struct fm_extent *fm_extents;
extern struct fm_inode *fm_inodes;

// For statistics
// Located in main.c
extern bool fm_integral_blksz;
extern uint64_t fm_extent_count;
extern uint64_t fm_inode_count;
extern uint64_t fm_file_count;
extern uint64_t fm_dir_count;

// Miscellaneous (initialised in main())
// Located in main.c
extern const char *argvzero;
extern uint64_t fm_blksz;

// Located in dirents.c
extern bool fm_scan_directory(int, const struct stat *restrict, const char *restrict) FM_NONNULL(2, 3) FM_WARN_UNUSED;

// Located in extents.c
extern bool fm_scan_extents(int, const struct stat *restrict, const char *restrict) FM_NONNULL(2, 3) FM_WARN_UNUSED;

// Located in options.c
extern enum fm_optparse_result fm_parse_options(int, char *[]) FM_NONNULL(2) FM_WARN_UNUSED;

// Located in print.c
extern void fm_print_message(const char *, ...) FM_NONNULL(1) FM_PRINTF(1, 2);
extern void fm_print_results(void);

// Located in sort.c
extern int fm_sortby_extent_cb(const void *restrict, const void *restrict) FM_NONNULL(1, 2);
extern int fm_sortby_filename_cb(const struct fm_name *restrict, const struct fm_name *restrict) FM_NONNULL(1, 2);

#endif /* !INC_FILEMAP_H */
