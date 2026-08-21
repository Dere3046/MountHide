// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>

#include "core.h"
#include "mh.h"

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
