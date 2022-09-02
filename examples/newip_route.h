/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Linux NewIP INET implementation
 *
 * Based on include/uapi/linux/ipv6_route.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _NEWIP_ROUTE_H
#define _NEWIP_ROUTE_H

#include "nip.h"

struct nip_arpmsg {
	struct nip_addr dst_addr;
	char ifrn_name[10];
	__u8 lladdr[10];
};

struct nip_rtmsg {
	struct nip_addr rtmsg_dst;
	struct nip_addr rtmsg_src;
	struct nip_addr rtmsg_gateway;
	char dev_name[10];
	__u32 rtmsg_type;
	int rtmsg_ifindex;
	__u32 rtmsg_metric;
	unsigned long rtmsg_info;
	__u32 rtmsg_flags;
};

#endif /* _NEWIP_ROUTE_H */
