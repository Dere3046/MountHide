// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_UMOUNT_H
#define MH_MH_UMOUNT_H

#include <linux/types.h>

int mh_umount_resolve(unsigned long (*resolve)(const char *name));

#endif
