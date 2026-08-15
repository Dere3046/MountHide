// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_VER_H
#define MH_MH_VER_H

#include <linux/types.h>

int mh_ver_init(unsigned long (*resolve)(const char *name));
void mh_ver_exit(void);
bool mh_scan_ready(void);

unsigned long mh_off_mnt_mp(void);
unsigned long mh_off_mp_dentry(void);
unsigned long mh_off_mnt(void);
unsigned long mh_off_mnt_mountpoint(void);
unsigned long mh_off_mnt_root(void);
unsigned long mh_off_mnt_instance(void);
unsigned long mh_off_sb_s_list(void);
unsigned long mh_off_sb_s_mounts(void);
unsigned long mh_off_vfs_mnt_root(void);
unsigned long mh_off_seqlock_lock(void);

int mh_safe_read(void *dst, const void *src, size_t len);

#endif
