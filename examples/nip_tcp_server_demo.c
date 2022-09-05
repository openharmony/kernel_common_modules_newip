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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include "nip_uapi.h"
#include "nip_lib.h"
#include "newip_route.h"

void *recv_send(void *args)
{
	char buf[BUFLEN] = {0};
	int cfd, ret;

	memcpy(&cfd, args, sizeof(int));
	for (int i = 0; i < PKTCNT; i++) {
		int recv_num = recv(cfd, buf, PKTLEN, MSG_WAITALL);

		if (recv_num < 0) {
			perror("recv");
			goto END;
		} else if (recv_num == 0) { /* no data */
			;
		} else {
			printf("Received -- %s --:%d\n", buf, recv_num);
			ret = send(cfd, buf, recv_num, 0);
			if (ret < 0) {
				perror("send");
				goto END;
			}
			printf("Sending  -- %s --:%d\n", buf, recv_num);
		}
	}
END:	close(cfd);
	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t th;
	int fd, cfd, addr_len;
	struct sockaddr_nin si_local;
	struct sockaddr_nin si_remote;

	fd = socket(AF_NINET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	memset((char *)&si_local, 0, sizeof(si_local));
	si_local.sin_family = AF_NINET;
	si_local.sin_port = htons(TCP_SERVER_PORT);
	// 2-byte address of the server: 0xDE00
	si_local.sin_addr.nip_addr_field8[INDEX_0] = 0xDE;
	si_local.sin_addr.nip_addr_field8[INDEX_1] = 0x00;
	si_local.sin_addr.bitlen = NIP_ADDR_BIT_LEN_16; // 2-byte: 16bit

	if (bind(fd, (const struct sockaddr *)&si_local, sizeof(si_local)) < 0) {
		perror("bind");
		goto END;
	}
	printf("bind success, addr=0x%02x%02x, port=%u\n",
	       si_local.sin_addr.nip_addr_field8[INDEX_0],
	       si_local.sin_addr.nip_addr_field8[INDEX_1], TCP_SERVER_PORT);

	if (listen(fd, LISTEN_MAX) < 0) {
		perror("listen");
		goto END;
	}

	addr_len = sizeof(si_remote);
	memset(&si_remote, 0, sizeof(si_remote));
	cfd = accept(fd, (struct sockaddr *)&si_remote, (socklen_t *)&addr_len);
	pthread_create(&th, NULL, recv_send, &cfd);
	/* Wait for the thread to end and synchronize operations between threads */
	pthread_join(th, NULL);
END:	close(fd);
	return 0;
}

