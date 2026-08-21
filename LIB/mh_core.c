// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/errno.h>

#include "hk.h"
#include "hk_patch.h"
#include "mh.h"
#include "mh_rule.h"
#include "mh_ver.h"
#include "mh_umount.h"
#include "mh_reg.h"

/* struct proc_mounts { ns(8) root(16) show(8) }, show offset is fixed */
#define MH_PROC_MOUNTS_SHOW_OFF 24

/* struct seq_operations { start next stop show }, show offset is fixed */
#define MH_SEQ_OPS_SHOW_OFF 24

struct proc_mounts;

static unsigned long mounts_op_addr;
static int (*orig_m_show)(struct seq_file *, void *);
static bool proc_hooked;
static bool inited;
static bool own_hk;

static mh_reader_fn_t reader_fn;
static mh_mount_fn_t extra_fn;

static unsigned int allow_uids[MH_UID_MAX];
static int allow_n;
static unsigned int hide_uids[MH_UID_MAX];
static int hide_n;

static bool mh_uid_in(const unsigned int *list, int n, unsigned int uid)
{
	int i;

	for (i = 0; i < n; i++) {
		if (list[i] == uid)
			return true;
	}
	return false;
}

int mh_reader_allow_uid(unsigned int uid)
{
	if (mh_uid_in(allow_uids, allow_n, uid))
		return 0;
	if (allow_n >= MH_UID_MAX)
		return -ENOSPC;
	allow_uids[allow_n++] = uid;
	return 0;
}

int mh_reader_hide_uid(unsigned int uid)
{
	if (mh_uid_in(hide_uids, hide_n, uid))
		return 0;
	if (hide_n >= MH_UID_MAX)
		return -ENOSPC;
	hide_uids[hide_n++] = uid;
	return 0;
}

void mh_reader_reset(void)
{
	allow_n = 0;
	hide_n = 0;
	reader_fn = NULL;
	extra_fn = NULL;
}

static bool mh_reader_default(void)
{
	return !uid_eq(current_uid(), GLOBAL_ROOT_UID);
}

static bool mh_need_hide_reader(void)
{
	unsigned int uid = from_kuid(&init_user_ns, current_uid());

	if (mh_uid_in(hide_uids, hide_n, uid))
		return true;
	if (mh_uid_in(allow_uids, allow_n, uid))
		return false;
	if (reader_fn)
		return reader_fn();
	return mh_reader_default();
}

static int __nocfi mh_mount_show(struct seq_file *m, void *v)
{
	struct proc_mounts *p = m->private;
	int (*show)(struct seq_file *, struct vfsmount *);
	unsigned long mnt_off;
	int err;

	err = mh_safe_read(&show, (const char *)p + MH_PROC_MOUNTS_SHOW_OFF,
			   sizeof(show));
	if (err || !show)
		return 0;

	if (mh_need_hide_reader()) {
		if (mh_rule_match(v))
			return 0;
		if (extra_fn && extra_fn(v))
			return 0;
	}

	mnt_off = mh_off_mnt();
	return show(m, (struct vfsmount *)((const char *)v + mnt_off));
}

int mh_init(const struct mh_cfg *cfg)
{
	struct hk_cfg hcfg;
	int ret;

	if (inited)
		return -EALREADY;

	if (!cfg || !cfg->resolve)
		return -EINVAL;

	hcfg.resolve = cfg->resolve;
	reader_fn = cfg->reader;
	extra_fn = cfg->extra;

	ret = hk_init(&hcfg);
	if (ret == -EALREADY) {
		pr_info("[mh] external hook stack already inited\n");
	} else if (ret) {
		return ret;
	} else {
		own_hk = true;
	}

	mounts_op_addr = hk_resolve("mounts_op");
	if (!mounts_op_addr) {
		if (own_hk)
			hk_exit();
		return -ENODATA;
	}

	ret = mh_ver_init((unsigned long (*)(const char *))hk_resolve);
	if (ret) {
		if (own_hk)
			hk_exit();
		return ret;
	}

	ret = mh_umount_resolve((unsigned long (*)(const char *))hk_resolve);
	if (ret)
		pr_info("[mh] path_umount unavailable, umount API disabled\n");

	ret = mh_reg_resolve((unsigned long (*)(const char *))hk_resolve);
	if (ret)
		pr_info("[mh] kern_path unavailable, hide_path disabled\n");

	ret = mh_safe_read(&orig_m_show,
			   (const void *)(mounts_op_addr + MH_SEQ_OPS_SHOW_OFF),
			   sizeof(orig_m_show));
	if (ret) {
		mh_ver_exit();
		if (own_hk)
			hk_exit();
		return ret;
	}

	inited = true;
	pr_info("[mh] inited, mounts_op=%lx orig_show=%ps\n", mounts_op_addr,
		orig_m_show);
	return 0;
}

void mh_exit(void)
{
	if (!inited)
		return;

	mh_proc_disable();
	mh_rule_clear();
	mh_reader_reset();
	mh_ver_exit();
	if (own_hk)
		hk_exit();
	mounts_op_addr = 0;
	orig_m_show = NULL;
	reader_fn = NULL;
	extra_fn = NULL;
	own_hk = false;
	inited = false;
}

int mh_proc_enable(void)
{
	if (!inited)
		return -EINVAL;
	if (proc_hooked)
		return 0;

	if (hk_patch_write((void *)(mounts_op_addr + MH_SEQ_OPS_SHOW_OFF),
			   (unsigned long)mh_mount_show))
		return -EIO;

	proc_hooked = true;
	pr_info("[mh] proc mount output hooked\n");
	return 0;
}

void mh_proc_disable(void)
{
	if (!inited || !proc_hooked)
		return;

	if (hk_patch_write((void *)(mounts_op_addr + MH_SEQ_OPS_SHOW_OFF),
			   (unsigned long)orig_m_show)) {
		pr_err("[mh] restore show failed\n");
		return;
	}
	proc_hooked = false;
	pr_info("[mh] proc mount output unhooked\n");
}
