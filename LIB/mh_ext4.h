// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 *
 * based on https://github.com/tiann/KernelSU
 */

#ifndef MH_MH_EXT4_H
#define MH_MH_EXT4_H

#include <linux/types.h>

struct dentry;
struct super_block;

int mh_ext4_resolve(unsigned long (*resolve)(const char *name));
void mh_ext4_exit(void);
bool mh_ext4_ready(void);

/* consumer owned switch, nuke calls are refused while false */
void mh_ext4_set_enabled(bool enabled);
bool mh_ext4_enabled(void);

/* shared dentry verdict, arg is struct dentry * */
bool mh_ext4_is_ext4(const void *dentry);

/* drop the sysfs entry of the ext4 super the dentry belongs to */
int mh_ext4_nuke_dentry(const struct dentry *dentry);

/* raw call on a super the consumer already holds, a second call is refused */
int mh_ext4_nuke_sb(const struct super_block *sb);

/* path based: resolve in the caller namespace, then nuke its super */
int mh_ext4_nuke_path(const char *mnt);

/* global view: every ext4 super, for mounts invisible to the caller */
int mh_ext4_nuke_all(void);

/* every path of the hidden mount table through mh_ext4_nuke_path */
int mh_ext4_nuke_hidden(void);

#endif
