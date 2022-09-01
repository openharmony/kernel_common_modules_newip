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

#include "nip.h"
#include "newip_route.h"

#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#define BUFLEN      4096

int g_pktcnt = 10;          // 发送报文个数
int g_pktlen = 1024;        // 发送报文长度
int g_pktinterval = 500;    // 发包间隔(ms)
int g_port = 5556;          // 服务端端口

typedef struct {
    int s;
    struct sockaddr_nin si_server;
} thread_args;

void *send_recv(void *args)
{
    char buf[BUFLEN];
    fd_set readfds;
    struct timeval tv, stTime;
    int sendtime_sec, sendtime_usec;
    int success = 0;
    int count = 0;
    int no = 0;
    thread_args *th_args = (thread_args *) args;
    int s = th_args->s;
    struct sockaddr_nin si_server = th_args->si_server;

    for (int i; i < g_pktcnt; i++) {
        memset(buf, 0, BUFLEN);
        (void)gettimeofday(&stTime, NULL);
        sprintf(buf, "%ld %6ld NIP_TCP # %6d", stTime.tv_sec, stTime.tv_usec, count);
        if (send(s, buf, g_pktlen, 0) < 0) {
            perror("send");
        }

        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        tv.tv_sec = 600;
        tv.tv_usec = 0;
        if (select(s + 1, &readfds, NULL, NULL, &tv) < 0) {
            perror("select");
        }

        if (FD_ISSET(s, &readfds)) {
            memset(buf, 0, BUFLEN);
            int ret = recv(s, buf, g_pktlen, MSG_WAITALL);
            if (ret > 0) {
                success += 1;
                (void)gettimeofday(&stTime, NULL);
                sscanf(buf, "%d %d NIP_TCP # %d", &sendtime_sec, &sendtime_usec, &no);
                printf("Received --%s sock %d success:%6d/%6d/no=%6d\n",
                       buf, s, success, count + 1, no);
            } else {
                printf("recv fail, ret=%d\n", ret);
            }
        }
        count += 1;
        usleep(g_pktinterval * 1000);
    }
    close(s);
}

int main(int argc, char **argv) 
{
    int s;
    pthread_t th;
    thread_args th_args;
    struct sockaddr_nin si_server;

    if ((s = socket(AF_NINET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printf("errno=%d\n", errno);
        return -1;
    }
    printf("creat newip socket, fd=%d\n", s);

    memset((char *)&si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_NINET;
    si_server.sin_port = htons(g_port);
    // 服务端2字节地址: 0xDE00
    si_server.sin_addr.nip_addr_field8[0] = 0xDE;
    si_server.sin_addr.nip_addr_field8[1] = 0x00;
    si_server.sin_addr.bitlen = 16; // 2字节：16bit
    if (connect(s, (struct sockaddr *)&si_server, sizeof(si_server)) < 0) {
        perror("connect");
        return -1;
    }
    printf("connect success, addr=0x%02x%02x, port=%u\n",
           si_server.sin_addr.nip_addr_field8[0], si_server.sin_addr.nip_addr_field8[1], g_port);

    th_args.si_server = si_server;
    th_args.si_server.sin_port = htons(g_port);
    th_args.s = s;
    pthread_create(&th, NULL, send_recv, &th_args);
    pthread_join(th, NULL);
    close(s);
    return 0;
}

