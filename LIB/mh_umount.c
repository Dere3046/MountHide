// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/errno.h>

#include "hk.h"
#include "mh.h"
#include "mh_rule.h"
#include "mh_reg.h"

static int (*path_umount_p)(struct path *path, int flags);

int __nocfi mh_umount_path(const char *mnt, int flags)
{
	struct path path;
	int err;

	if (!path_umount_p)
		return -ENOSYS;

	err = mh_kern_path(mnt, LOOKUP_FOLLOW, &path);
	if (err)
		return err;

	err = path_umount_p(&path, flags);
	mh_path_put(&path);
	return err;
}

static int mh_umount_cb(const char *path, void *arg)
{
	int flags = *(int *)arg;
	int err = mh_umount_path(path, flags);

	pr_info("[mh] umount %s flags 0x%x -> %d\n", path, flags, err);
	return 0;
}

int mh_umount_all(int flags)
{
	if (!path_umount_p)
		return -ENOSYS;

	return mh_rule_foreach(mh_umount_cb, &flags);
}

int mh_umount_resolve(unsigned long (*resolve)(const char *name))
{
	path_umount_p = (int (*)(struct path *, int))resolve("path_umount");
	if (!path_umount_p)
		return -ENODATA;
	return 0;
}
