// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 *
 * based on https://github.com/tiann/KernelSU
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "hk.h"
#include "mh.h"
#include "mh_ext4.h"
#include "mh_reg.h"
#include "mh_rule.h"
#include "mh_ver.h"

#define MH_EXT4_MAX_SB 16

static void (*ext4_unregister_sysfs_p)(struct super_block *sb);
static void (*iterate_supers_type_p)(struct file_system_type *type,
				     void (*fn)(struct super_block *, void *),
				     void *arg);
static struct file_system_type *(*get_fs_type_p)(const char *name);
static void (*put_filesystem_p)(struct file_system_type *fs);

static struct super_block *ext4_sb[MH_EXT4_MAX_SB];
static unsigned long ext4_sb_done[BITS_TO_LONGS(MH_EXT4_MAX_SB)];
static int ext4_sb_n;

static bool ext4_disabled;

bool mh_ext4_ready(void)
{
	return ext4_unregister_sysfs_p != NULL;
}

void mh_ext4_set_enabled(bool enabled)
{
	ext4_disabled = !enabled;
}

bool mh_ext4_enabled(void)
{
	return !ext4_disabled;
}

static bool mh_ext4_sb_done(const struct super_block *sb)
{
	int i;

	for (i = 0; i < MH_EXT4_MAX_SB; i++) {
		if (test_bit(i, ext4_sb_done) && ext4_sb[i] == sb)
			return true;
	}
	return false;
}

static int mh_ext4_sb_mark(const struct super_block *sb)
{
	int i;

	for (i = 0; i < MH_EXT4_MAX_SB; i++) {
		if (!test_and_set_bit(i, ext4_sb_done)) {
			ext4_sb[i] = (struct super_block *)sb;
			return 0;
		}
	}
	return -ENOSPC;
}

static struct super_block *mh_ext4_sb_of(const void *dentry)
{
	unsigned long off_inode = mh_off_dentry_d_inode();
	unsigned long off_sb = mh_off_inode_i_sb();
	struct dentry *d;
	struct inode *inode;
	struct super_block *sb;

	if (!dentry || !off_inode || !off_sb)
		return NULL;
	if (mh_safe_read(&d, dentry, sizeof(d)) || !d)
		return NULL;
	if (mh_safe_read(&inode, (const char *)d + off_inode, sizeof(inode)))
		return NULL;
	if (!inode)
		return NULL;
	if (mh_safe_read(&sb, (const char *)inode + off_sb, sizeof(sb)))
		return NULL;
	return sb;
}

bool mh_ext4_is_ext4(const void *dentry)
{
	struct super_block *sb = mh_ext4_sb_of(dentry);
	struct file_system_type *type;
	char name[16];

	if (!sb || !sb->s_type || !sb->s_type->name)
		return false;
	type = sb->s_type;
	if (mh_safe_read(name, type->name, sizeof(name)))
		return false;
	name[sizeof(name) - 1] = 0;
	return !strcmp(name, "ext4");
}

int __nocfi mh_ext4_nuke_sb(const struct super_block *sb)
{
	if (!ext4_unregister_sysfs_p)
		return -ENOSYS;
	if (ext4_disabled)
		return -EPERM;
	if (!sb)
		return -EINVAL;
	/* a second unregister would touch a released kobject */
	if (mh_ext4_sb_done(sb))
		return -EALREADY;

	mh_ext4_sb_mark(sb);
	ext4_unregister_sysfs_p((struct super_block *)sb);
	pr_info("[mh] ext4 sysfs nuked sb %px\n", sb);
	return 0;
}

int mh_ext4_nuke_dentry(const struct dentry *dentry)
{
	return mh_ext4_nuke_sb(mh_ext4_sb_of(dentry));
}

int mh_ext4_nuke_path(const char *mnt)
{
	struct path path;
	struct super_block *sb;
	int err;

	if (!ext4_unregister_sysfs_p)
		return -ENOSYS;
	if (ext4_disabled)
		return -EPERM;
	if (!mnt)
		return -EINVAL;

	err = mh_kern_path(mnt, 0, &path);
	if (err)
		return err;

	sb = mh_ext4_sb_of(path.dentry);
	if (!mh_ext4_is_ext4(path.dentry)) {
		pr_info("[mh] %s is not ext4\n", mnt);
		mh_path_put(&path);
		return -EINVAL;
	}

	err = mh_ext4_nuke_sb(sb);
	mh_path_put(&path);
	return err;
}

static void mh_ext4_collect(struct super_block *sb, void *arg)
{
	(void)arg;
	if (ext4_sb_n >= MH_EXT4_MAX_SB)
		return;
	ext4_sb[ext4_sb_n++] = sb;
}

int __nocfi mh_ext4_nuke_all(void)
{
	struct file_system_type *type;
	int i;
	int hits = 0;

	if (!ext4_unregister_sysfs_p)
		return -ENOSYS;
	if (ext4_disabled)
		return -EPERM;
	if (!iterate_supers_type_p || !get_fs_type_p || !put_filesystem_p)
		return -ENODATA;

	type = get_fs_type_p("ext4");
	if (!type) {
		pr_info("[mh] ext4 fs type unavailable\n");
		return -ENODATA;
	}

	ext4_sb_n = 0;
	memset(ext4_sb_done, 0, sizeof(ext4_sb_done));
	iterate_supers_type_p(type, mh_ext4_collect, NULL);
	put_filesystem_p(type);

	for (i = 0; i < ext4_sb_n; i++) {
		if (mh_ext4_nuke_sb(ext4_sb[i]))
			continue;
		hits++;
	}
	pr_info("[mh] ext4 sysfs nuke all -> %d supers\n", hits);
	return hits;
}

int mh_ext4_nuke_hidden(void)
{
	char paths[MH_RULE_BATCH_MAX * MH_RULE_PATH_MAX];
	int off = 0;
	int hits = 0;
	int n;
	int i;
	int err;

	if (!ext4_unregister_sysfs_p)
		return -ENOSYS;
	if (ext4_disabled)
		return -EPERM;

	/* chunks of MH_RULE_BATCH_MAX keep the frame inside the kernel limit */
	for (;;) {
		n = mh_rule_snapshot(paths, off, MH_RULE_BATCH_MAX);
		if (n < 0)
			return n;
		if (!n)
			break;
		for (i = 0; i < n; i++) {
			err = mh_ext4_nuke_path(mh_rule_snapshot_at(paths, i));
			if (!err)
				hits++;
		}
		off += n;
		if (n < MH_RULE_BATCH_MAX)
			break;
	}
	pr_info("[mh] ext4 sysfs nuke hidden -> %d of %d\n", hits, off);
	return hits;
}

int mh_ext4_resolve(unsigned long (*resolve)(const char *name))
{
	ext4_unregister_sysfs_p =
		(void (*)(struct super_block *))resolve("ext4_unregister_sysfs");
	iterate_supers_type_p =
		(void (*)(struct file_system_type *,
			  void (*)(struct super_block *, void *),
			  void *))resolve("iterate_supers_type");
	get_fs_type_p = (struct file_system_type *(*)(const char *))resolve(
		"get_fs_type");
	put_filesystem_p =
		(void (*)(struct file_system_type *))resolve("put_filesystem");

	if (!ext4_unregister_sysfs_p)
		return -ENODATA;
	return 0;
}

void mh_ext4_exit(void)
{
	ext4_unregister_sysfs_p = NULL;
	iterate_supers_type_p = NULL;
	get_fs_type_p = NULL;
	put_filesystem_p = NULL;
	ext4_sb_n = 0;
	memset(ext4_sb_done, 0, sizeof(ext4_sb_done));
}
