// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_H
#define MH_MH_H

#include <linux/types.h>

#define MH_UID_MAX 16

/* reader rule: return true to hide hidden mounts from current reader */
typedef bool (*mh_reader_fn_t)(void);

/* extra mount rule: return true to hide this mount, arg is struct mount * */
typedef bool (*mh_mount_fn_t)(const void *mnt);

struct mh_cfg {
	unsigned long (*resolve)(const char *name); /* required, __nocfi wrapper of kallrecon_klp */
	mh_reader_fn_t reader; /* NULL: default, hide from non root readers */
	mh_mount_fn_t extra;   /* NULL: disabled */
};

int mh_init(const struct mh_cfg *cfg);
void mh_exit(void);

/* reader visibility: hide list wins over allow list over defaults */
int mh_reader_allow_uid(unsigned int uid);
int mh_reader_hide_uid(unsigned int uid);
void mh_reader_reset(void);

/* proc output hiding: /proc/pid/{mountinfo,mounts,mountstats} */
int mh_proc_enable(void);
void mh_proc_disable(void);

/* hidden mountpoint list, shared by proc hiding and umount */
int mh_hide_path(const char *path);
int mh_unhide_path(const char *path);
void mh_hide_clear(void);

/* global view scan: auto register mounts whose root path contains feature */
int mh_hide_scan(const char *feature);

/* real umount API, timing is fully consumer defined */
int mh_umount_path(const char *mnt, int flags);
int mh_umount_all(int flags);

/* true if this mount matches the hidden list, arg is struct mount * */
bool mh_is_hidden(const void *mnt);

#endif
