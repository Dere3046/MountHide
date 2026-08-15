// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/dcache.h>
#include <linux/errno.h>

#include "mh.h"
#include "mh_ver.h"

struct mh_entry {
	struct list_head list;
	struct dentry *dentry; /* mountpoint dentry, matching key */
	char *path;            /* for umount */
};

static LIST_HEAD(mh_entries);
static DEFINE_SPINLOCK(mh_lock);

static struct mh_entry *mh_entry_find(const char *path)
{
	struct mh_entry *e;

	list_for_each_entry (e, &mh_entries, list) {
		if (!strcmp(e->path, path))
			return e;
	}
	return NULL;
}

int mh_rule_add(const char *path, struct dentry *dentry)
{
	struct mh_entry *e;
	unsigned long flags;

	if (!path || !dentry)
		return -EINVAL;

	spin_lock_irqsave(&mh_lock, flags);
	e = mh_entry_find(path);
	if (e) {
		spin_unlock_irqrestore(&mh_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&mh_lock, flags);

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return -ENOMEM;
	e->path = kstrdup(path, GFP_KERNEL);
	if (!e->path) {
		kfree(e);
		return -ENOMEM;
	}
	e->dentry = dget(dentry);

	spin_lock_irqsave(&mh_lock, flags);
	if (mh_entry_find(path)) {
		spin_unlock_irqrestore(&mh_lock, flags);
		dput(e->dentry);
		kfree(e->path);
		kfree(e);
		return 0;
	}
	list_add_tail(&e->list, &mh_entries);
	spin_unlock_irqrestore(&mh_lock, flags);

	return 0;
}

int mh_rule_del(const char *path)
{
	struct mh_entry *e;
	unsigned long flags;

	spin_lock_irqsave(&mh_lock, flags);
	e = mh_entry_find(path);
	if (!e) {
		spin_unlock_irqrestore(&mh_lock, flags);
		return -ENOENT;
	}
	list_del(&e->list);
	spin_unlock_irqrestore(&mh_lock, flags);

	dput(e->dentry);
	kfree(e->path);
	kfree(e);
	return 0;
}

void mh_rule_clear(void)
{
	struct mh_entry *e, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&mh_lock, flags);
	list_for_each_entry_safe (e, tmp, &mh_entries, list) {
		list_del(&e->list);
		dput(e->dentry);
		kfree(e->path);
		kfree(e);
	}
	spin_unlock_irqrestore(&mh_lock, flags);
}

bool mh_rule_match(const void *mnt)
{
	struct mh_entry *e;
	struct mountpoint *mp;
	struct dentry *d;
	const char *base = mnt;
	unsigned long mnt_mp = mh_off_mnt_mp();
	unsigned long mp_dentry = mh_off_mp_dentry();
	bool found = false;
	unsigned long flags;

	if (!mnt_mp || !mp_dentry)
		return false;

	if (mh_safe_read(&mp, base + mnt_mp, sizeof(mp)))
		return false;
	if (!mp)
		return false;
	if (mh_safe_read(&d, (const char *)mp + mp_dentry, sizeof(d)))
		return false;
	if (!d)
		return false;

	spin_lock_irqsave(&mh_lock, flags);
	list_for_each_entry (e, &mh_entries, list) {
		if (e->dentry == d) {
			found = true;
			break;
		}
	}
	spin_unlock_irqrestore(&mh_lock, flags);

	return found;
}

int mh_rule_count(void)
{
	struct mh_entry *e;
	int n = 0;
	unsigned long flags;

	spin_lock_irqsave(&mh_lock, flags);
	list_for_each_entry (e, &mh_entries, list)
		n++;
	spin_unlock_irqrestore(&mh_lock, flags);

	return n;
}

int mh_rule_foreach(int (*cb)(const char *path, void *arg), void *arg)
{
	struct mh_entry *e;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&mh_lock, flags);
	list_for_each_entry (e, &mh_entries, list) {
		ret = cb(e->path, arg);
		if (ret)
			break;
	}
	spin_unlock_irqrestore(&mh_lock, flags);

	return ret;
}
