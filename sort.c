/*
 * SPDX-License-Identifier: ISC
 * SPDX-URL: https://spdx.org/licenses/ISC.html
 *
 * Copyright (C) 2023 Aaron M. D. Jones <me@aaronmdjones.net>
 */

#include <stddef.h>
#include <string.h>

#include "filemap.h"

static int FM_NONNULL(1, 2)
fm_sortby_extoff_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->off < in2->off)
		return -1;

	if (in1->off > in2->off)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_extlen_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->len < in2->len)
		return -1;

	if (in1->len > in2->len)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_extcnt_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->inode->extcount < in2->inode->extcount)
		return -1;

	if (in1->inode->extcount > in2->inode->extcount)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_inofcnt_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->inode->namecount < in2->inode->namecount)
		return -1;

	if (in1->inode->namecount > in2->inode->namecount)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_inonum_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->inode->inum < in2->inode->inum)
		return -1;

	if (in1->inode->inum > in2->inode->inum)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_inofsz_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	if (in1->inode->sb.st_size < in2->inode->sb.st_size)
		return -1;

	if (in1->inode->sb.st_size > in2->inode->sb.st_size)
		return 1;

	return 0;
}

static int FM_NONNULL(1, 2)
fm_sortby_inofname_cb(const struct fm_extent *const restrict in1, const struct fm_extent *const restrict in2)
{
	const int ret = strcmp(in1->inode->names->name, in2->inode->names->name);

	if (ret < 0)
		return -1;

	if (ret > 0)
		return 1;

	return 0;
}

int FM_NONNULL(1, 2)
fm_sortby_extent_cb(const void *const restrict ptr1, const void *const restrict ptr2)
{
	const struct fm_extent *const in1 = ptr1;
	const struct fm_extent *const in2 = ptr2;
	int ret;

	switch (fm_sort_method)
	{
		case FM_SORTMETH_EXTENT_OFFSET:
		{
			ret = fm_sortby_extoff_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_EXTENT_LENGTH:
		{
			ret = fm_sortby_extlen_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_INODE_EXTENT_COUNT:
		{
			ret = fm_sortby_extcnt_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_INODE_LINK_COUNT:
		{
			ret = fm_sortby_inofcnt_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_INODE_NUMBER:
		{
			ret = fm_sortby_inonum_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_FILESIZE:
		{
			ret = fm_sortby_inofsz_cb(in1, in2);
			break;
		}
		case FM_SORTMETH_FILENAME:
		{
			ret = fm_sortby_inofname_cb(in1, in2);
			break;
		}
	}

	switch (fm_sort_direction)
	{
		case FM_SORTDIR_ASCENDING:
		{
			return ret;
		}
		case FM_SORTDIR_DESCENDING:
		{
			if (ret < 0)
				return 1;

			if (ret > 0)
				return -1;

			return 0;
		}
	}

	return 0;
}

int FM_NONNULL(1, 2)
fm_sortby_filename_cb(const struct fm_name *const restrict in1, const struct fm_name *const restrict in2)
{
	const int ret = strcmp(in1->name, in2->name);

	if (ret < 0)
		return -1;

	if (ret > 0)
		return 1;

	return 0;
}
