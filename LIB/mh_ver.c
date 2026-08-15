// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/errno.h>

#include "btf.h"
#include "type_info.h"
#include "mh_ver.h"

static unsigned long mnt_mp_off;
static unsigned long mp_dentry_off;
static unsigned long mnt_off;
static unsigned long mnt_mountpoint_off;
static unsigned long mnt_instance_off;
static unsigned long sb_s_list_off;
static unsigned long sb_s_mounts_off;
static unsigned long vfs_mnt_root_off;
static unsigned long seqlock_lock_off = 4;
static bool scan_ready;
static struct ti_ctx *btf;

int mh_safe_read(void *dst, const void *src, size_t len)
{
	return copy_from_kernel_nofault(dst, src, len);
}

unsigned long mh_off_mnt_mp(void)
{
	return mnt_mp_off;
}

unsigned long mh_off_mp_dentry(void)
{
	return mp_dentry_off;
}

unsigned long mh_off_mnt(void)
{
	return mnt_off;
}

unsigned long mh_off_mnt_mountpoint(void)
{
	return mnt_mountpoint_off;
}

unsigned long mh_off_mnt_root(void)
{
	return vfs_mnt_root_off;
}

unsigned long mh_off_mnt_instance(void)
{
	return mnt_instance_off;
}

unsigned long mh_off_sb_s_list(void)
{
	return sb_s_list_off;
}

unsigned long mh_off_sb_s_mounts(void)
{
	return sb_s_mounts_off;
}

unsigned long mh_off_seqlock_lock(void)
{
	return seqlock_lock_off;
}

bool mh_scan_ready(void)
{
	return scan_ready;
}

int mh_ver_init(unsigned long (*resolve)(const char *name))
{
	struct ti_resolver res = {
		.name_to_addr = resolve,
	};
	u32 id, bit_off, bit_sz;
	int ret;

	ret = ti_init(&res);
	if (ret)
		return ret;

	btf = ti_base();
	if (!btf)
		return -ENODATA;

	ret = ti_type_by_name(btf, "mount", BIT(BTF_KIND_STRUCT), &id);
	if (ret)
		goto err;
	{
		u32 mount_id = id;

		ret = ti_member_off(btf, mount_id, "mnt_mp", &bit_off, &bit_sz);
		if (ret)
			goto err;
		mnt_mp_off = bit_off / 8;

		ret = ti_member_off(btf, mount_id, "mnt", &bit_off, &bit_sz);
		if (ret)
			goto err;
		mnt_off = bit_off / 8;

		ret = ti_member_off(btf, mount_id, "mnt_mountpoint", &bit_off,
				    &bit_sz);
		if (ret)
			goto err;
		mnt_mountpoint_off = bit_off / 8;

		if (!ti_member_off(btf, mount_id, "mnt_instance", &bit_off,
				   &bit_sz))
			mnt_instance_off = bit_off / 8;
		else
			pr_info("[mh] mount.mnt_instance unavailable\n");
	}

	if (!ti_type_by_name(btf, "vfsmount", BIT(BTF_KIND_STRUCT), &id)) {
		int vret = ti_member_off(btf, id, "mnt_root", &bit_off,
					 &bit_sz);

		if (!vret)
			vfs_mnt_root_off = bit_off / 8;
		else
			pr_info("[mh] vfsmount.mnt_root off failed %d\n",
				vret);
	} else {
		pr_info("[mh] vfsmount btf unavailable\n");
	}

	if (!ti_type_by_name(btf, "super_block", BIT(BTF_KIND_STRUCT), &id)) {
		int sret = ti_member_off(btf, id, "s_list", &bit_off, &bit_sz);

		if (!sret)
			sb_s_list_off = bit_off / 8;
		else
			pr_info("[mh] super_block.s_list off failed %d\n",
				sret);
		if (!ti_member_off(btf, id, "s_mounts", &bit_off, &bit_sz))
			sb_s_mounts_off = bit_off / 8;
		else
			pr_info("[mh] super_block.s_mounts unavailable\n");
	} else {
		pr_info("[mh] super_block btf unavailable\n");
	}

	if (!ti_type_by_name(btf, "seqlock_t", BIT(BTF_KIND_TYPEDEF), &id)) {
		if (!ti_follow(btf, id, &id) &&
		    !ti_member_off(btf, id, "lock", &bit_off, &bit_sz))
			seqlock_lock_off = bit_off / 8;
	}

	ret = ti_type_by_name(btf, "mountpoint", BIT(BTF_KIND_STRUCT), &id);
	if (ret)
		goto err;
	ret = ti_member_off(btf, id, "m_dentry", &bit_off, &bit_sz);
	if (ret)
		goto err;
	mp_dentry_off = bit_off / 8;

	pr_info("[mh] offs mnt_mp=%lu mnt=%lu mntpoint=%lu instance=%lu "
		"vfs_root=%lu s_list=%lu s_mounts=%lu mp_dentry=%lu\n",
		mnt_mp_off, mnt_off, mnt_mountpoint_off, mnt_instance_off,
		vfs_mnt_root_off, sb_s_list_off, sb_s_mounts_off,
		mp_dentry_off);

	scan_ready = true;

	return 0;

err:
	ti_exit();
	btf = NULL;
	return ret;
}

void mh_ver_exit(void)
{
	ti_exit();
	btf = NULL;
	mnt_mp_off = 0;
	mp_dentry_off = 0;
	mnt_off = 0;
	mnt_mountpoint_off = 0;
	mnt_instance_off = 0;
	sb_s_list_off = 0;
	sb_s_mounts_off = 0;
	vfs_mnt_root_off = 0;
	seqlock_lock_off = 4;
	scan_ready = false;
}
