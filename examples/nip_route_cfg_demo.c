// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET socket protocol family
 */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/route.h>

#include "nip_linux.h"
#include "newip_route.h"
#include "nip_api.h"

/* 基于设备名字获取ifindex
 * struct ifreq ifr;
 * struct nip_ifreq ifrn;
 * ioctl(fd, SIOGIFINDEX, &ifr);
 * ifr.ifr_ifindex; ===> 函数入参 ifindex
 */
int32_t nip_route_add(int32_t ifindex, const unsigned char *dst_addr, uint8_t dst_addr_len,
	const unsigned char *gateway_addr, uint8_t gateway_addr_len)
{
	int32_t sockfd, ret;
	struct nip_rtmsg rt;

	sockfd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (sockfd < 0)
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

	ret = ioctl(sockfd, SIOCADDRT, &rt);
	if (ret < 0 && errno != EEXIST) { // ignore File Exists error
		close(sockfd);
		return -1;
	}

	close(sockfd);
	return 0;
}

/* 执行用例前先执行ifconfig xxx up，xxx表示网卡名，比如eth0，wlan0 */
int main(int argc, char **argv)
{
	int ret;
	int32_t ifindex = 0;
	const char ifname[] = {"eth0"};              // eth0, wlan0可选，根据实际接口修改
	uint8_t client_addr[INDEX_1] = {0x50};       // 客户端1字节地址: 0x50
	uint8_t server_addr[INDEX_2] = {0xDE, 0x00}; // 服务端2字节地址: 0xDE00
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

	ret = nip_get_ifindex(ifname, &ifindex);
	if (ret != 0) {
		printf("get %s ifindex fail, ret=%d.\n", ifname, ret);
		return -1;
	}

	ret = nip_route_add(ifindex, dst_addr, dst_addr_len, NULL, 0);
	if (ret != 0) {
		printf("get %s ifindex fail, ret=%d.\n", ifname, ret);
		return -1;
	}

	printf("%s %s(ifindex=%u) cfg route success.\n", *argv, ifname, ifindex);
	return 0;
}

