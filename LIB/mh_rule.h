// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_RULE_H
#define MH_MH_RULE_H

#include <linux/types.h>

struct dentry;

int mh_rule_add(const char *path, struct dentry *dentry);
int mh_rule_del(const char *path);
void mh_rule_clear(void);
bool mh_rule_match(const void *mnt);
int mh_rule_count(void);
int mh_rule_foreach(int (*cb)(const char *path, void *arg), void *arg);

#endif
