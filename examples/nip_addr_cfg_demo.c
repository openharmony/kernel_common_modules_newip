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

#include "nip_linux.h"
#include "nip_api.h"

/* 基于设备名字获取ifindex
 * struct ifreq ifr;
 * struct nip_ifreq ifrn;
 * ioctl(fd, SIOGIFINDEX, &ifr);
 * ifr.ifr_ifindex; ===> 函数入参 ifindex
 */
int32_t nip_add_addr(int32_t ifindex, const unsigned char *addr, uint8_t addr_len)
{
	int32_t sockfd, ret;
	struct nip_ifreq ifrn;

	sockfd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		return -1;

	memset(&ifrn, 0, sizeof(ifrn));
	ifrn.ifrn_addr.bitlen = addr_len * 8; // 字节长度转换成bit长度
	memcpy(ifrn.ifrn_addr.nip_addr_field8, addr, addr_len);
	ifrn.ifrn_ifindex = ifindex;

	ret = ioctl(sockfd, SIOCSIFADDR, &ifrn);
	if (ret < 0 && errno != EEXIST) { // ignore File Exists error
		printf("cfg newip addr fail, ifindex=%d, ret=%d.\n", ifindex, ret);
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
	uint8_t *addr;
	uint8_t addr_len;

	if (argc == DEMO_INPUT_1) {
		if (!strcmp(*(argv + 1), "server")) {
			printf("server cfg addr=0x%02x%02x.\n",
			       server_addr[INDEX_0], server_addr[INDEX_1]);
			addr = server_addr;
			addr_len = sizeof(server_addr);
		} else if (!strcmp(*(argv + 1), "client")) {
			printf("client cfg addr=0x%x02x.\n", client_addr[INDEX_0]);
			addr = client_addr;
			addr_len = sizeof(client_addr);
		} else {
			printf("invalid addr cfg input.\n");
			return -1;
		}
	} else {
		printf("unsupport addr cfg input.\n");
		return -1;
	}

	ret = nip_get_ifindex(ifname, &ifindex);
	if (ret != 0)
		return -1;

	ret = nip_add_addr(ifindex, addr, addr_len);
	if (ret != 0)
		return -1;

	printf("%s %s(ifindex=%u) cfg addr success.\n", *argv, ifname, ifindex);
	return 0;
}

