// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/printk.h>
#include <linux/string.h>

#include "core.h"
#include "mh.h"

#define MH_GATE_ALLOW_MAX 16
#define MH_GATE_UMOUNT_FLAGS (MNT_DETACH)

char *hide_mounts[64] = { "/debug_ramdisk" };
int hide_count = 1;
module_param_array(hide_mounts, charp, &hide_count, 0444);
MODULE_PARM_DESC(hide_mounts, "mountpoints to hide from non root readers");

char *scan_feature = "/adb/";
module_param(scan_feature, charp, 0444);
MODULE_PARM_DESC(scan_feature, "root path feature to auto hide, empty disables");

char *allow_uids = "";
module_param(allow_uids, charp, 0444);
MODULE_PARM_DESC(allow_uids, "comma separated uids that always see the mounts");

char *hide_uids = "";
module_param(hide_uids, charp, 0444);
MODULE_PARM_DESC(hide_uids, "comma separated uids that never see the mounts");

char *umount_uids = "";
module_param(umount_uids, charp, 0444);
MODULE_PARM_DESC(umount_uids, "comma separated uids the gate may umount for");

bool umount_gate;
module_param(umount_gate, bool, 0444);
MODULE_PARM_DESC(umount_gate, "arm the umount gate, requires a zygote domain sid");

bool nuke_ext4 = true;
module_param(nuke_ext4, bool, 0444);
MODULE_PARM_DESC(nuke_ext4, "drop the ext4 sysfs entries of the hidden mounts");

static kuid_t gate_uids[MH_GATE_ALLOW_MAX];
static int gate_uid_n;

static bool mh_src_domain_check(const struct cred *cred)
{
	u32 sid = mh_umount_sid(cred);

	return sid && sid == mh_umount_sid_get();
}

static bool mh_src_should_umount(uid_t uid)
{
	return false;
}

static struct mh_umount_cfg gate_cfg = {
	.domain_check = mh_src_domain_check,
	.should_umount = mh_src_should_umount,
	.allow_uids = gate_uids,
	.allow_n = 0,
};

static void mh_parse_uids(const char *str, int (*fn)(unsigned int))
{
	char buf[128];
	char *p;
	char *tok;

	if (!str || !*str)
		return;
	strscpy(buf, str, sizeof(buf));
	p = buf;
	while ((tok = strsep(&p, ",")) != NULL) {
		unsigned int uid;

		if (!*tok)
			continue;
		if (kstrtouint(tok, 10, &uid))
			continue;
		fn(uid);
	}
}

static void mh_parse_gate_uids(const char *str)
{
	char buf[128];
	char *p;
	char *tok;

	if (!str || !*str)
		return;
	strscpy(buf, str, sizeof(buf));
	p = buf;
	while ((tok = strsep(&p, ",")) != NULL) {
		unsigned int uid;

		if (!*tok)
			continue;
		if (kstrtouint(tok, 10, &uid))
			continue;
		if (gate_uid_n >= MH_GATE_ALLOW_MAX)
			break;
		gate_uids[gate_uid_n++] = KUIDT_INIT(uid);
	}
	gate_cfg.allow_n = gate_uid_n;
}

static int mh_src_gate_setup(void)
{
	int ret;

	/* init_cred is the only cred that is valid before the first fork */
	ret = mh_umount_set_cred(&init_cred);
	if (ret) {
		pr_err("[mhsrc] gate cred failed %d\n", ret);
		return ret;
	}

	ret = mh_umount_capture_sid();
	if (ret) {
		pr_err("[mhsrc] zygote sid capture failed %d\n", ret);
		return ret;
	}

	mh_parse_gate_uids(umount_uids);
	ret = mh_umount_cfg_init(&gate_cfg);
	if (ret)
		return ret;

	mh_umount_set_module_mounted(true);
	pr_info("[mhsrc] gate armed, zygote sid %u, %d uids\n",
		mh_umount_sid_get(), gate_uid_n);
	return 0;
}

static unsigned long __nocfi kr_name_to_addr(const char *name)
{
	if (kallrecon_klp)
		return kallrecon_klp(name);
	return 0;
}

static int __init mh_src_init(void)
{
	struct mh_cfg cfg = {
		.resolve = kr_name_to_addr,
	};
	int ret, i;

	find_kallsyms_base();
	if (!klnum_val || !kallrecon_klp) {
		pr_err("[mhsrc] kallsyms recovery failed\n");
		return -ENODATA;
	}

	ret = mh_init(&cfg);
	if (ret) {
		pr_err("[mhsrc] mh_init failed %d\n", ret);
		return ret;
	}

	mh_parse_uids(allow_uids, mh_reader_allow_uid);
	mh_parse_uids(hide_uids, mh_reader_hide_uid);

	ret = mh_proc_enable();
	if (ret) {
		pr_err("[mhsrc] mh_proc_enable failed %d\n", ret);
		mh_exit();
		return ret;
	}

	for (i = 0; i < hide_count; i++) {
		ret = mh_hide_path(hide_mounts[i]);
		pr_info("[mhsrc] hide %s -> %d\n", hide_mounts[i], ret);
	}

	if (scan_feature && *scan_feature) {
		ret = mh_hide_scan(scan_feature);
		pr_info("[mhsrc] scan %s -> %d\n", scan_feature, ret);
	}

	if (nuke_ext4 && mh_ext4_ready()) {
		ret = mh_ext4_nuke_hidden();
		pr_info("[mhsrc] ext4 sysfs nuke -> %d\n", ret);
	}

	if (umount_gate) {
		ret = mh_src_gate_setup();
		if (ret)
			pr_info("[mhsrc] gate setup failed %d\n", ret);
	}

	pr_info("[mhsrc] loaded\n");
	return 0;
}

static void __exit mh_src_exit(void)
{
	mh_exit();
	pr_info("[mhsrc] unloaded\n");
}

module_init(mh_src_init);
module_exit(mh_src_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mount hiding consumer demo for PrivIsolated");
