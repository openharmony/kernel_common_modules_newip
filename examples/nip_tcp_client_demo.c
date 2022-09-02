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
#include <sys/time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "nip_linux.h"
#include "newip_route.h"

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#define BUFLEN          4096
#define LISTEN_MAX      3
#define PORT            5556    // 服务端端口
#define PKTCNT          10      // 发送报文个数
#define PKTLEN          1024    // 发送报文长度
#define SLEEP_US        500000  // 发包间隔(ms)
#define SELECT_TIME     600

struct thread_args {
	int s;
	struct sockaddr_nin si_server;
};

void *send_recv(void *args)
{
	int ret;
	char buf[BUFLEN];
	fd_set readfds;
	struct timeval tv, stTime;
	int sendtime_sec, sendtime_usec;
	int success = 0;
	int count = 0;
	int no = 0;
	struct thread_args *th_args = (struct thread_args *) args;
	int s = th_args->s;
	struct sockaddr_nin si_server = th_args->si_server;

	for (int i; i < PKTCNT; i++) {
		memset(buf, 0, BUFLEN);
		(void)gettimeofday(&stTime, NULL);
		sprintf(buf, "%ld %6ld NIP_TCP # %6d", stTime.tv_sec, stTime.tv_usec, count);
		if (send(s, buf, PKTLEN, 0) < 0)
			perror("send");

		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		tv.tv_sec = SELECT_TIME;
		tv.tv_usec = 0;
		if (select(s + 1, &readfds, NULL, NULL, &tv) < 0)
			perror("select");

		if (FD_ISSET(s, &readfds)) {
			memset(buf, 0, BUFLEN);
			ret = recv(s, buf, PKTLEN, MSG_WAITALL);
			if (ret > 0) {
				success += 1;
				(void)gettimeofday(&stTime, NULL);
				ret = sscanf(buf, "%d %d NIP_TCP # %d",
					     &sendtime_sec, &sendtime_usec, &no);
				printf("Received --%s sock %d success:%6d/%6d/no=%6d\n",
				       buf, s, success, count + 1, no);
			} else {
				printf("recv fail, ret=%d\n", ret);
			}
		}
		count += 1;
		usleep(SLEEP_US);
	}
	close(s);
}

int main(int argc, char **argv)
{
	int s;
	pthread_t th;
	struct thread_args th_args;
	struct sockaddr_nin si_server;

	s = socket(AF_NINET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		perror("socket");
		return -1;
	}
	printf("creat newip socket, fd=%d\n", s);

	memset((char *)&si_server, 0, sizeof(si_server));
	si_server.sin_family = AF_NINET;
	si_server.sin_port = htons(PORT);
	// 服务端2字节地址: 0xDE00
	si_server.sin_addr.nip_addr_field8[INDEX_0] = 0xDE;
	si_server.sin_addr.nip_addr_field8[INDEX_1] = 0x00;
	si_server.sin_addr.bitlen = NIP_ADDR_BIT_LEN_16; // 2字节：16bit
	if (connect(s, (struct sockaddr *)&si_server, sizeof(si_server)) < 0) {
		perror("connect");
		return -1;
	}
	printf("connect success, addr=0x%02x%02x, port=%u\n",
	       si_server.sin_addr.nip_addr_field8[INDEX_0],
	       si_server.sin_addr.nip_addr_field8[INDEX_1], PORT);

	th_args.si_server = si_server;
	th_args.si_server.sin_port = htons(PORT);
	th_args.s = s;
	pthread_create(&th, NULL, send_recv, &th_args);
	pthread_join(th, NULL);
	close(s);
	return 0;
}

