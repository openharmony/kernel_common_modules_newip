// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET socket protocol family
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include "nip_linux.h"
#include "newip_route.h"

#define BUFLEN          4096
#define PKTCNT_SCALE    2
#define LISTEN_MAX      3
#define PORT            5556    // 服务端端口
#define PKTCNT          10      // 发送报文个数
#define PKTLEN          1024    // 发送报文长度

void *recv_send(void *args)
{
	char buf[BUFLEN] = {0};
	int client, i, err;

	memcpy(&client, args, sizeof(int));
	for (i = 0; i < PKTCNT * PKTCNT_SCALE; i++) {
		int recvNum = recv(client, buf, PKTLEN, MSG_WAITALL);

		if (recvNum <= 0) {
			perror("recv");
		} else {
			printf("Received -- %s --:%d\n", buf, recvNum);
			err = send(client, buf, recvNum, 0);
			if (err < 0) {
				perror("send");
				break;
			}
			printf("Sending  -- %s --:%d\n", buf, recvNum);
		}
	}
	close(client);
}

int main(int argc, char **argv)
{
	pthread_t th;
	int s, fd, addr_len;
	struct sockaddr_nin si_local;
	struct sockaddr_nin si_remote;

	s = socket(AF_NINET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		perror("socket");
		return -1;
	}
	printf("creat newip socket, fd=%d\n", s);

	memset((char *)&si_local, 0, sizeof(si_local));
	si_local.sin_family = AF_NINET;
	si_local.sin_port = htons(PORT);
	// 服务端2字节地址: 0xDE00
	si_local.sin_addr.nip_addr_field8[INDEX_0] = 0xDE;
	si_local.sin_addr.nip_addr_field8[INDEX_1] = 0x00;
	si_local.sin_addr.bitlen = NIP_ADDR_BIT_LEN_16; // 2字节：16bit

	if (bind(s, (const struct sockaddr *)&si_local, sizeof(si_local)) < 0) {
		perror("bind");
		return -1;
	}
	printf("bind success, addr=0x%02x%02x, port=%u\n",
	       si_local.sin_addr.nip_addr_field8[INDEX_0],
	       si_local.sin_addr.nip_addr_field8[INDEX_1], PORT);

	if (listen(s, LISTEN_MAX) < 0) {
		perror("listen");
		return -1;
	}
	printf("listen success.\n");

	addr_len = sizeof(si_remote);
	memset(&si_remote, 0, sizeof(si_remote));
	fd = accept(s, (struct sockaddr *)&si_remote, (socklen_t *)&addr_len);
	pthread_create(&th, NULL, recv_send, &fd);
	pthread_join(th, NULL); // 等待线程结束, 线程间同步的操作
	close(s);
	return 0;
}

