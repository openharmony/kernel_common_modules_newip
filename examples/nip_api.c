// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP INET socket protocol family
 */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "nip_linux.h"
#include "nip_api.h"

int32_t nip_get_ifindex(const char *ifname, int32_t *ifindex)
{
	int fd;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	fd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (fd < 0) {
		printf("creat socket fail, ifname=%s\n", ifname);
		return -1;
	}
	if ((ioctl(fd, SIOCGIFINDEX, &ifr)) < 0) {
		printf("get ifindex fail, ifname=%s\n", ifname);
		close(fd);
		return -1;
	}
	close(fd);

	printf("%s ifindex=%u\n", ifname, ifr.ifr_ifindex);
	*ifindex = ifr.ifr_ifindex;
	return 0;
}

int32_t set_netcard_up(const char *ifname)
{
	int32_t fd;
	struct ifreq ifr;

	fd = socket(AF_NINET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		printf("get %s flag fail.", ifname);
		close(fd);
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		printf("set %s up fail.", ifname);
		close(fd);
		return -1;
	}

	printf("set %s up success.", ifname);
	close(fd);
	return 0;
}

