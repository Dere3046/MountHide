// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 *
 * based on https://github.com/tiann/KernelSU
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/errno.h>

#include "hk.h"
#include "mh.h"
#include "mh_gate.h"
#include "mh_umount.h"

#define MH_ISOLATED_FIRST 99000
#define MH_ISOLATED_LAST 99999
#define MH_APP_FIRST 10000
#define MH_APP_LAST 19999

static mh_domain_fn_t domain_check;
static mh_should_umount_fn_t should_umount;
static kuid_t *allow_uids;
static int allow_n;

static bool module_mounted;
static bool cfg_done;

static const struct cred *gate_cred;
static bool cred_owned;

static const struct cred *(*override_creds_p)(const struct cred *cred);
static void (*revert_creds_p)(const struct cred *cred);
static void (*cred_getsecid_p)(const struct cred *cred, u32 *secid);

static u32 zygote_sid;

bool mh_umount_gate_ready(void)
{
	return cfg_done && domain_check && gate_cred && override_creds_p &&
	       revert_creds_p && mh_umount_resolved();
}

u32 __nocfi mh_umount_sid(const struct cred *cred)
{
	u32 sid = 0;

	if (!cred || !cred_getsecid_p)
		return 0;
	cred_getsecid_p(cred, &sid);
	return sid;
}

int mh_umount_sid_set(const struct cred *cred)
{
	u32 sid = mh_umount_sid(cred);

	if (!sid)
		return -ENODATA;
	zygote_sid = sid;
	return 0;
}

int mh_umount_capture_sid(void)
{
	return mh_umount_sid_set(current_cred());
}

u32 mh_umount_sid_get(void)
{
	return zygote_sid;
}

static bool mh_uid_is_app(uid_t uid)
{
	uid_t app = uid % MH_PER_USER_RANGE;

	return app >= MH_APP_FIRST && app <= MH_APP_LAST;
}

static bool mh_uid_is_isolated(uid_t uid)
{
	uid_t app = uid % MH_PER_USER_RANGE;

	return app >= MH_ISOLATED_FIRST && app <= MH_ISOLATED_LAST;
}

static bool mh_uid_in_allow(uid_t uid)
{
	kuid_t kuid = KUIDT_INIT(uid);
	int i;

	for (i = 0; i < allow_n; i++) {
		if (uid_eq(allow_uids[i], kuid))
			return true;
	}
	return false;
}

int mh_umount_set_cred(const struct cred *cred)
{
	struct cred *new;

	if (cred_owned && gate_cred)
		put_cred(gate_cred);
	gate_cred = NULL;
	cred_owned = false;

	if (!cred)
		return 0;

	/*
	 * init_cred is const and permanent, a copy is taken so the gate owns
	 * something it may release and so the mount ns is the init namespace
	 */
	new = prepare_creds();
	if (!new)
		return -ENOMEM;
	new->uid = cred->uid;
	new->gid = cred->gid;
	new->euid = cred->euid;
	new->egid = cred->egid;
	new->suid = cred->suid;
	new->sgid = cred->sgid;
	new->fsuid = cred->fsuid;
	new->fsgid = cred->fsgid;
	new->securebits = cred->securebits;
	new->cap_effective = cred->cap_effective;
	new->cap_permitted = cred->cap_permitted;
	new->cap_bset = cred->cap_bset;
	new->cap_ambient = cred->cap_ambient;
	gate_cred = new;
	cred_owned = true;
	return 0;
}

int mh_umount_cfg_init(const struct mh_umount_cfg *cfg)
{
	override_creds_p =
		(const struct cred *(*)(const struct cred *))hk_resolve(
			"override_creds");
	revert_creds_p =
		(void (*)(const struct cred *))hk_resolve("revert_creds");
	cred_getsecid_p = (void (*)(const struct cred *, u32 *))hk_resolve(
		"security_cred_getsecid");

	domain_check = cfg ? cfg->domain_check : NULL;
	should_umount = cfg ? cfg->should_umount : NULL;
	allow_uids = cfg ? cfg->allow_uids : NULL;
	allow_n = cfg && cfg->allow_n > 0 ? cfg->allow_n : 0;
	zygote_sid = 0;
	cfg_done = true;

	if (!domain_check)
		pr_info("[mh] gate domain check not injected, gate disabled\n");
	if (!override_creds_p || !revert_creds_p)
		pr_info("[mh] gate cred override unavailable\n");
	if (!cred_getsecid_p)
		pr_info("[mh] gate selinux sid lookup unavailable\n");

	return 0;
}

void mh_umount_set_module_mounted(bool mounted)
{
	module_mounted = mounted;
}

static void __nocfi mh_gate_umount_hidden(int flags)
{
	const struct cred *saved;

	saved = override_creds_p(gate_cred);
	mh_umount_all(flags);
	revert_creds_p(saved);
}

int mh_umount_gate(uid_t uid, int flags)
{
	bool isolated = mh_uid_is_isolated(uid);
	bool allowed;

	if (!module_mounted)
		return 0;
	if (!cfg_done || !domain_check)
		return -ENOTSUPP;

	if (!mh_uid_is_app(uid) && !isolated && uid != MH_WEBVIEW_ZYGOTE_UID)
		return 0;

	if (uid != current_uid().val)
		return -EINVAL;
	if (should_umount) {
		allowed = should_umount(uid);
	} else {
		allowed = mh_uid_in_allow(uid);
	}
	if (!allowed && !isolated)
		return 0;
	if (!mh_umount_gate_ready())
		return -ENOSYS;

	/*
	 * the domain still carried by current cred belongs to the process that
	 * forked us, so a su app that setuid away from a shell is rejected here
	 */
	if (!domain_check(current_cred())) {
		pr_info("[mh] gate ignore non zygote child %d\n",
			task_pid_nr(current));
		return 0;
	}

	pr_info("[mh] gate umount for uid %d pid %d\n", uid,
		task_pid_nr(current));
	mh_gate_umount_hidden(flags);
	return 0;
}

void mh_gate_exit(void)
{
	if (cred_owned && gate_cred)
		put_cred(gate_cred);
	gate_cred = NULL;
	cred_owned = false;
	domain_check = NULL;
	should_umount = NULL;
	allow_uids = NULL;
	allow_n = 0;
	module_mounted = false;
	cfg_done = false;
	override_creds_p = NULL;
	revert_creds_p = NULL;
	cred_getsecid_p = NULL;
	zygote_sid = 0;
}
