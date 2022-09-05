// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/route.h>

#include "nip_uapi.h"
#include "nip_lib.h"
#include "newip_route.h"

/* get ifindex based on the device name
 * struct ifreq ifr;
 * struct nip_ifreq ifrn;
 * ioctl(fd, SIOGIFINDEX, &ifr);
 * ifr.ifr_ifindex; ===> ifindex
 */
int nip_route_add(int ifindex, const unsigned char *dst_addr, uint8_t dst_addr_len,
	const unsigned char *gateway_addr, uint8_t gateway_addr_len)
{
	int fd, ret;
	struct nip_rtmsg rt;

	fd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	memset(&rt, 0, sizeof(rt));
	rt.rtmsg_ifindex = ifindex;
	rt.rtmsg_flags = RTF_UP;
	rt.rtmsg_dst.bitlen = dst_addr_len * 8;
	memcpy(rt.rtmsg_dst.nip_addr_field8, dst_addr, dst_addr_len);

	if (gateway_addr) {
		rt.rtmsg_gateway.bitlen = gateway_addr_len * 8;
		memcpy(rt.rtmsg_gateway.nip_addr_field8, gateway_addr, gateway_addr_len);
		rt.rtmsg_flags |= RTF_GATEWAY;
	}

	ret = ioctl(fd, SIOCADDRT, &rt);
	if (ret < 0 && errno != EEXIST) { // ignore File Exists error
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	int ifindex = 0;
	uint8_t client_addr[INDEX_1] = {0x50};       // 1-byte address of the client: 0x50
	uint8_t server_addr[INDEX_2] = {0xDE, 0x00}; // 2-byte address of the server: 0xDE00
	uint8_t *dst_addr;
	uint8_t dst_addr_len;
	uint8_t *gateway_addr;
	uint8_t gateway_addr_len;

	if (argc == DEMO_INPUT_1) {
		if (!strcmp(*(argv + 1), "server")) {
			printf("server cfg route, dst-addr=0x%x02.\n", client_addr[INDEX_0]);
			dst_addr = client_addr;
			dst_addr_len = 1;
		} else if (!strcmp(*(argv + 1), "client")) {
			printf("client cfg route, dst-addr=0x%02x%02x.\n",
			       server_addr[INDEX_0], server_addr[INDEX_1]);
			dst_addr = server_addr;
			dst_addr_len = 2;
		} else {
			printf("invalid route cfg input.\n");
			return -1;
		}
	} else {
		printf("unsupport route cfg input.\n");
		return -1;
	}

	ret = nip_get_ifindex(NIC_NAME, &ifindex);
	if (ret != 0) {
		printf("get %s ifindex fail, ret=%d.\n", NIC_NAME, ret);
		return -1;
	}

	ret = nip_route_add(ifindex, dst_addr, dst_addr_len, NULL, 0);
	if (ret != 0) {
		printf("get %s ifindex fail, ret=%d.\n", NIC_NAME, ret);
		return -1;
	}

	printf("%s %s(ifindex=%u) cfg route success.\n", *argv, NIC_NAME, ifindex);
	return 0;
}

