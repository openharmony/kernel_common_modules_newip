/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Linux NewIP INET implementation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _NIP_API_H
#define _NIP_API_H

int32_t nip_get_ifindex(const char *ifname, int32_t *ifindex);

#endif /* _NIP_API_H */
