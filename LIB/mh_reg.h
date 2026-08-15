// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 dere3046
 */

#ifndef MH_MH_REG_H
#define MH_MH_REG_H

#include <linux/types.h>

struct path;

int mh_reg_resolve(unsigned long (*resolve)(const char *name));
int mh_kern_path(const char *name, unsigned int flags, struct path *path);
void mh_path_put(struct path *path);

#endif
