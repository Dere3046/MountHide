// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/errno.h>

#include "hk.h"
#include "mh.h"
#include "mh_rule.h"
#include "mh_ver.h"

static int (*kern_path_p)(const char *name, unsigned int flags,
			  struct path *path);
static void (*path_put_p)(struct path *path);

int __nocfi mh_kern_path(const char *name, unsigned int flags,
			 struct path *path)
{
	if (!kern_path_p)
		return -ENOSYS;
	return kern_path_p(name, flags, path);
}

void __nocfi mh_path_put(struct path *path)
{
	if (path_put_p)
		path_put_p(path);
}

int mh_reg_resolve(unsigned long (*resolve)(const char *name))
{
	kern_path_p = (int (*)(const char *, unsigned int,
			       struct path *))resolve("kern_path");
	if (!kern_path_p)
		return -ENODATA;
	path_put_p = (void (*)(struct path *))resolve("path_put");
	return 0;
}

int mh_hide_path(const char *path)
{
	struct path p;
	struct dentry *mp_dentry = NULL;
	unsigned long mnt_off, mp_off;
	const char *r;
	int err;

	if (!path)
		return -EINVAL;

	err = mh_kern_path(path, LOOKUP_FOLLOW, &p);
	if (err)
		return err;

	mnt_off = mh_off_mnt();
	mp_off = mh_off_mnt_mountpoint();
	r = (const char *)p.mnt - mnt_off;
	err = mh_safe_read(&mp_dentry, r + mp_off, sizeof(mp_dentry));
	mh_path_put(&p);
	if (err || !mp_dentry)
		return err ? err : -ENODATA;

	err = mh_rule_add(path, mp_dentry);
	return err;
}

int mh_unhide_path(const char *path)
{
	if (!path)
		return -EINVAL;

	return mh_rule_del(path);
}

void mh_hide_clear(void)
{
	mh_rule_clear();
}

bool mh_is_hidden(const void *mnt)
{
	return mh_rule_match(mnt);
}
