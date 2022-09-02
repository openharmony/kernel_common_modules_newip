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

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include "nip_linux.h"
#include "newip_route.h"

#define BUFLEN      4096
#define PKT_NUM     10      // 收发包次数
#define SLEEP_US    500000  // 发包间隔(ms)
#define PORT        9090

struct thread_args {
	int s;
	struct sockaddr_nin si_server;
};

void *send_recv(void *args)
{
	int ret;
	char buf[BUFLEN];
	fd_set readfds;
	struct timeval tv;
	int success = 0;
	int count = 0;
	int no = 0;
	struct timeval stTime;
	int sendtime_sec, sendtime_usec;
	struct thread_args *th_args = (struct thread_args *)args;
	int s = th_args->s;
	struct sockaddr_nin si_server = th_args->si_server;

	while (count < PKT_NUM) {
		socklen_t slen = sizeof(si_server);

		memset(buf, 0, BUFLEN);
		gettimeofday(&stTime, NULL);
		sprintf(buf, "%ld %6ld NIP_UDP # %6d", stTime.tv_sec, stTime.tv_usec, count);

		if (sendto(s, buf, BUFLEN, 0, (struct sockaddr *)&si_server, slen) < 0)
			perror("sendto");

		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if (select(s + 1, &readfds, NULL, NULL, &tv) < 0)
			perror("select");

		if (FD_ISSET(s, &readfds)) {
			memset(buf, 0, BUFLEN);
			ret = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_server, &slen);
			if (ret > 0) {
				success += 1;
				(void)gettimeofday(&stTime, NULL);
				ret = sscanf(buf, "%d %d NIP_UDP # %d", &sendtime_sec,
					     &sendtime_usec, &no);
				printf("Received --%s sock %d success:%6d/%6d/no=%6d\n",
					   buf, s, success, count + 1, no);
			} else {
				printf("recv fail, ret=%d\n", ret);
			}
		}
		count += 1;
		usleep(SLEEP_US);
	}
}

int main(int argc, char **argv)
{
	int s;
	pthread_t th;
	struct thread_args th_args;
	struct sockaddr_nin si_server;

	s = socket(AF_NINET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	memset((char *)&si_server, 0, sizeof(si_server));
	si_server.sin_family = AF_NINET;
	si_server.sin_port = htons(PORT);
	// 服务端2字节地址: 0xDE00
	si_server.sin_addr.nip_addr_field8[INDEX_0] = 0xDE;
	si_server.sin_addr.nip_addr_field8[INDEX_1] = 0x00;
	si_server.sin_addr.bitlen = NIP_ADDR_BIT_LEN_16; // 2字节：16bit

	th_args.si_server = si_server;
	th_args.si_server.sin_port = htons(PORT);
	th_args.s = s;
	pthread_create(&th, NULL, send_recv, &th_args);
	pthread_join(th, NULL);
	close(s);
	return 0;
}

