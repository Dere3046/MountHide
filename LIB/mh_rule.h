// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_RULE_H
#define MH_MH_RULE_H

#include <linux/types.h>

struct dentry;

/* entry and path limits of the snapshot used by non atomic walkers */
#define MH_RULE_BATCH_MAX 8
#define MH_RULE_PATH_MAX 128

int mh_rule_add(const char *path, struct dentry *dentry);
int mh_rule_del(const char *path);
void mh_rule_clear(void);
bool mh_rule_match(const void *mnt);
int mh_rule_count(void);
int mh_rule_foreach(int (*cb)(const char *path, void *arg), void *arg);

/*
 * copy up to max paths starting at index off into a caller owned buffer of
 * max * MH_RULE_PATH_MAX bytes, the lock is dropped before returning so the
 * caller may sleep per path
 */
int mh_rule_snapshot(void *buf, int off, int max);
const char *mh_rule_snapshot_at(const void *buf, int idx);

#endif
