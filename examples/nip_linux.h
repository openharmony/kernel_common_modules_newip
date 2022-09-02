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
#ifndef _NIP_LINUX_H
#define _NIP_LINUX_H

#include <linux/if.h>
#include "nip.h"

// AF_NINET可通过读取 /sys/module/newip/parameters/af_ninet 文件来获取类型数值。
#define PF_NINET 45
#define AF_NINET PF_NINET

#define DEMO_INPUT_1  2  // DEMO程序包含1个入参S

/* The following structure must be larger than V4. System calls use V4.
 * If the definition is smaller than V4, the read process will have memory overruns
 * v4: include\linux\socket.h --> sockaddr (16Byte)
 */
#define POD_SOCKADDR_SIZE 3
struct sockaddr_nin {
	unsigned short sin_family; /* [2Byte] AF_NINET */
	unsigned short sin_port;   /* [2Byte] Transport layer port, big-endian */
	struct nip_addr sin_addr;  /* [9Byte] NIP address */

	unsigned char sin_zero[POD_SOCKADDR_SIZE]; /* [3Byte] Byte alignment */
};

struct nip_ifreq {
	struct nip_addr ifrn_addr;
	int ifrn_ifindex;
};

#endif /*_NIP_LINUX_H*/
