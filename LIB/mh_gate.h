// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_GATE_H
#define MH_MH_GATE_H

#include <linux/types.h>
#include <linux/cred.h>
#include <linux/uidgid.h>

#define MH_PER_USER_RANGE 100000
#define MH_WEBVIEW_ZYGOTE_UID 1053

/*
 * domain verdict for one cred: true when the cred still carries the domain
 * it had before the fork, arg is never NULL
 */
typedef bool (*mh_domain_fn_t)(const struct cred *cred);

/* uid policy verdict: true when a process of this uid may drop the mounts */
typedef bool (*mh_should_umount_fn_t)(uid_t uid);

/*
 * domain_check is the safety interlock: a process that switched uid but kept
 * a non zygote domain (su apps, adb) must never reach the umount primitive
 */
struct mh_umount_cfg {
	mh_domain_fn_t domain_check; /* required, NULL disables the gate */
	mh_should_umount_fn_t should_umount; /* NULL: allow list only */
	kuid_t *allow_uids; /* uid policy, may be NULL */
	int allow_n;
};

int mh_umount_cfg_init(const struct mh_umount_cfg *cfg);
void mh_gate_exit(void);

/* false until ready and domain_check are both set */
bool mh_umount_gate_ready(void);

/* sid of one cred via security_cred_getsecid, 0 when unresolved or LSM off */
u32 mh_umount_sid(const struct cred *cred);

/*
 * low level helpers, only needed by consumers that build their own domain
 * check on selinux ids instead of the selinuxid library
 */
int mh_umount_capture_sid(void);
int mh_umount_sid_set(const struct cred *cred);
u32 mh_umount_sid_get(void);

int mh_umount_set_cred(const struct cred *cred);

/* consumer owned switch, short circuits the gate when false */
void mh_umount_set_module_mounted(bool mounted);

/*
 * call from the setresuid path with the uid just committed and the flags the
 * hidden mounts should be unmounted with, returns 0 when the gate skipped
 */
int mh_umount_gate(uid_t uid, int flags);

#endif
