// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/atomic.h>

#include "hk.h"
#include "mh_rule.h"
#include "mh_ver.h"

#define MH_SCAN_MAX 64
#define QSPIN_LOCKED_VAL 1

struct mh_qspinlock {
	union {
		atomic_t val;
		struct {
			u8 locked;
			u8 pending;
		};
	};
};

static const char *mh_scan_feature;
static struct dentry *mh_scan_q[MH_SCAN_MAX];
static int mh_scan_q_n;

static unsigned long mount_lock_addr;
static void (*qspin_slowpath)(struct mh_qspinlock *lock, u32 val);
static char *(*dentry_path_raw_p)(const struct dentry *dentry, char *buf,
				  int buflen);

static char *__nocfi mh_dentry_path_raw(const struct dentry *dentry,
					char *buf, int buflen)
{
	if (!dentry_path_raw_p)
		return ERR_PTR(-ENOSYS);
	return dentry_path_raw_p(dentry, buf, buflen);
}

static void __nocfi mh_lock_mount_hash(void)
{
	struct mh_qspinlock *lock =
		(struct mh_qspinlock *)(mount_lock_addr + mh_off_seqlock_lock());
	u32 val;

	val = atomic_cmpxchg_acquire(&lock->val, 0, QSPIN_LOCKED_VAL);
	if (unlikely(val))
		qspin_slowpath(lock, val);
}

static void mh_unlock_mount_hash(void)
{
	struct mh_qspinlock *lock =
		(struct mh_qspinlock *)(mount_lock_addr + mh_off_seqlock_lock());

	smp_store_release(&lock->locked, 0);
}

static int mh_scan_dentry_path(const void *dentry_slot, char *buf, int buflen)
{
	unsigned long d;
	char *p;

	if (mh_safe_read(&d, dentry_slot, sizeof(d)))
		return -EFAULT;
	if (!d)
		return -ENOENT;
	p = mh_dentry_path_raw((struct dentry *)d, buf, buflen);
	if (IS_ERR(p))
		return PTR_ERR(p);
	if (p != buf)
		memmove(buf, p, strlen(p) + 1);
	return 0;
}

static void mh_scan_sb(struct super_block *sb, void *arg)
{
	unsigned long mnt_head;
	unsigned long mnt_cur;
	unsigned long mnt_instance = mh_off_mnt_instance();
	unsigned long mnt_root = mh_off_mnt_root();
	unsigned long mnt_mountpoint = mh_off_mnt_mountpoint();
	unsigned long mnt_off = mh_off_mnt();
	char root_buf[512];

	if (mh_scan_q_n >= MH_SCAN_MAX)
		return;

	mnt_head = (unsigned long)sb + mh_off_sb_s_mounts();
	if (mh_safe_read(&mnt_cur, (void *)mnt_head, sizeof(mnt_cur)))
		return;

	mh_lock_mount_hash();
	while (mnt_cur != mnt_head && mnt_cur &&
	       mh_scan_q_n < MH_SCAN_MAX) {
		const char *mnt = (const char *)(mnt_cur - mnt_instance);
		struct dentry *mp_dentry = NULL;

		if (mh_safe_read(&mp_dentry, mnt + mnt_mountpoint,
				 sizeof(mp_dentry)))
			break;
		if (!mp_dentry)
			goto next;

		if (!mh_scan_dentry_path(mnt + mnt_off + mnt_root, root_buf,
					 sizeof(root_buf)) &&
		    strstr(root_buf, mh_scan_feature)) {
			dget(mp_dentry);
			mh_scan_q[mh_scan_q_n++] = mp_dentry;
			pr_info("[mh] scan hit %s\n", root_buf);
		}
next:
		if (mh_safe_read(&mnt_cur, (void *)mnt_cur, sizeof(mnt_cur)))
			break;
	}
	mh_unlock_mount_hash();
}

static int mh_scan_register(void)
{
	char point_buf[512];
	char *p;
	int i;
	int err;

	for (i = 0; i < mh_scan_q_n; i++) {
		p = mh_dentry_path_raw(mh_scan_q[i], point_buf,
				    sizeof(point_buf));
		if (IS_ERR(p)) {
			dput(mh_scan_q[i]);
			continue;
		}
		if (p != point_buf)
			memmove(point_buf, p, strlen(p) + 1);
		err = mh_rule_add(point_buf, mh_scan_q[i]);
		if (err)
			pr_info("[mh] scan add %s failed %d\n", point_buf,
				err);
		dput(mh_scan_q[i]);
	}
	return mh_scan_q_n;
}

int __nocfi mh_hide_scan(const char *feature)
{
	void (*iterate_supers_fn)(void (*)(struct super_block *, void *),
				  void *) = NULL;
	unsigned long addr;
	int hits;

	if (!feature || !*feature)
		return -EINVAL;
	if (!mh_scan_ready())
		return -ENODATA;

	addr = hk_resolve("iterate_supers");
	if (!addr)
		return -ENODATA;
	iterate_supers_fn = (void (*)(void (*)(struct super_block *, void *),
				      void *))addr;

	if (!mount_lock_addr) {
		addr = hk_resolve("mount_lock");
		if (!addr)
			return -ENODATA;
		mount_lock_addr = addr;
	}
	if (!qspin_slowpath) {
		addr = hk_resolve("queued_spin_lock_slowpath");
		if (!addr)
			return -ENODATA;
		qspin_slowpath = (void (*)(struct mh_qspinlock *, u32))addr;
	}
	if (!dentry_path_raw_p) {
		addr = hk_resolve("dentry_path_raw");
		if (!addr)
			return -ENODATA;
		dentry_path_raw_p = (char *(*)(const struct dentry *, char *,
					       int))addr;
	}

	mh_scan_feature = feature;
	mh_scan_q_n = 0;
	iterate_supers_fn(mh_scan_sb, NULL);
	hits = mh_scan_register();

	pr_info("[mh] scan done, %d mounts hidden by %s\n", hits, feature);
	return hits;
}
