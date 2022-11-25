// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "nip_uapi.h"
#include "nip_lib.h"

/* get ifindex based on the device name
 * struct ifreq ifr;
 * struct nip_ifreq ifrn;
 * ioctl(fd, SIOGIFINDEX, &ifr);
 * ifr.ifr_ifindex; ===> ifindex
 */
int32_t nip_add_addr(int32_t ifindex, const unsigned char *addr, uint8_t addr_len)
{
	int fd, ret;
	struct nip_ifreq ifrn;

	fd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	memset(&ifrn, 0, sizeof(ifrn));
	ifrn.ifrn_addr.bitlen = addr_len * 8; // Byte length is converted to bit length
	memcpy(ifrn.ifrn_addr.nip_addr_field8, addr, addr_len);
	ifrn.ifrn_ifindex = ifindex;

	ret = ioctl(fd, SIOCSIFADDR, &ifrn);
	if (ret < 0 && errno != EEXIST) { // ignore File Exists error
		printf("cfg newip addr fail, ifindex=%d, ret=%d.\n", ifindex, ret);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

/* Before executing the use case, run ifconfig XXX up.
 * XXX indicates the NIC name, for example, eth0 and wlan0
 */
int main(int argc, char **argv)
{
	int ret;
	int ifindex = 0;
	uint8_t client_addr[INDEX_1] = {0x50};       // 1-byte address of the client: 0x50
	uint8_t server_addr[INDEX_2] = {0xDE, 0x00}; // 2-byte address of the server: 0xDE00
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

	ret = nip_get_ifindex(NIC_NAME, &ifindex);
	if (ret != 0)
		return -1;

	ret = nip_add_addr(ifindex, addr, addr_len);
	if (ret != 0)
		return -1;

	printf("%s %s(ifindex=%d) cfg addr success.\n", *argv, NIC_NAME, ifindex);
	return 0;
}

