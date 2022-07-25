// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP common UDP code
 * Linux NewIP INET implementation
 *
 * Adapted from linux/net/ipv6/datagram.c
 */
#include <net/addrconf.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/socket.h>

int nip_datagram_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	int res = 0;
	return res;
}

void nip_datagram_release_cb(struct sock *sk)
{
	;
}

