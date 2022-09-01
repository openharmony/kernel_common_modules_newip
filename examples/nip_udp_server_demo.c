// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET socket protocol family
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "nip.h"
#include "newip_route.h"

#define BUFLEN      1024

int main(int argc, char **argv)
{
    int s;
    socklen_t slen;
    int port = 9090;
    char buf[BUFLEN];
    struct sockaddr_nin si_local, si_remote;

    if ((s = socket(AF_NINET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        return -1;
    }

    memset((char *)&si_local, 0, sizeof(si_local));
    si_local.sin_family = AF_NINET;
    si_local.sin_port = htons(port);
    // 服务端2字节地址: 0xDE00
    si_local.sin_addr.nip_addr_field8[0] = 0xDE;
    si_local.sin_addr.nip_addr_field8[1] = 0x00;
    si_local.sin_addr.bitlen = 16; // 2字节：16bit

    if (bind(s, (const struct sockaddr *)&si_local, sizeof(si_local)) == -1){
        perror("bind");
        close(s);
        return -1;
    }

    while (1) {
        slen = sizeof(si_remote);
        memset(buf, 0, sizeof(char) * BUFLEN);
        memset(&si_remote, 0, sizeof(si_remote));

        if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_remote, &slen) == -1) {
            perror("recvfrom");
            break;
        } else {
            printf("Received -- %s -- from 0x%x:%d\n", buf,
                   si_remote.sin_addr.nip_addr_field16[0], ntohs(si_remote.sin_port));
            slen = sizeof(si_remote);

            if (sendto(s, buf, BUFLEN, 0, (struct sockaddr *)&si_remote, slen) == -1) {
                perror("sendto");
                break;
            }
            printf("Sending  -- %s -- to 0x%0x:%d\n", buf,
                   si_remote.sin_addr.nip_addr_field8[0], ntohs(si_remote.sin_port));
        }
    }

    close(s);
    exit(EXIT_SUCCESS);
}

