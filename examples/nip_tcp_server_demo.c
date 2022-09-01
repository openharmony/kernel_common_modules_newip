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

#include "nip.h"
#include "newip_route.h"

#define BUFLEN   4096

int g_pktcnt = 10;          // 发送报文个数
int g_pktlen = 1024;        // 发送报文长度
int g_port = 5556;          // 服务端端口

void* recv_send(void* args){
    char buf[BUFLEN] = {0};
    int client, i, err;

    memcpy(&client, args, sizeof(int));
    for (i = 0; i < g_pktcnt * 2; i++) {
        int recvNum = recv(client, buf, g_pktlen, MSG_WAITALL);

        if (recvNum < 0) {
            perror("recv");
        } else if (recvNum == 0) {
            break;
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

    if ((s = socket(AF_NINET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket");
        return -1;
    }
    printf("creat newip socket, fd=%d\n", s);

    memset((char *)&si_local, 0, sizeof(si_local));
    si_local.sin_family = AF_NINET;
    si_local.sin_port = htons(g_port);
    // 服务端2字节地址: 0xDE00
    si_local.sin_addr.nip_addr_field8[0] = 0xDE;
    si_local.sin_addr.nip_addr_field8[1] = 0x00;
    si_local.sin_addr.bitlen = 16; // 2字节：16bit

    if (bind(s, (const struct sockaddr *)&si_local, sizeof(si_local)) < 0) {
        perror("bind");
        return -1;
    }
    printf("bind success, addr=0x%02x%02x, port=%u\n",
           si_local.sin_addr.nip_addr_field8[0], si_local.sin_addr.nip_addr_field8[1], g_port);

    if (listen(s, 3) < 0) {
        perror("listen");
        return -1;
    }
    printf("listen success.\n");

    addr_len = sizeof(si_remote);
    memset(&si_remote, 0, sizeof(si_remote));
    fd = accept(s, (struct sockaddr*)&si_remote, (socklen_t*)&addr_len);
    pthread_create(&th, NULL, recv_send, &fd);
    pthread_join(th, NULL); // 等待线程结束, 线程间同步的操作
    close(s);
    return 0;
}

