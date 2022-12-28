// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Definitions for the NewIP Hooks Register module.
 */
#ifdef CONFIG_NEWIP_HOOKS
#define pr_fmt(fmt) KBUILD_MODNAME ": [%s:%d] " fmt, __func__, __LINE__

#include <net/ninet_hashtables.h>      /* ninet_ehashfn */
#include <net/if_ninet.h>
#include <trace/hooks/nip_hooks.h>
#include "tcp_nip_parameter.h"

void ninet_ehashfn_hook(void *data, const struct sock *sk, u32 *ret)
{
	*ret = ninet_ehashfn(sock_net(sk), &sk->sk_nip_rcv_saddr,
			     sk->sk_num, &sk->sk_nip_daddr, sk->sk_dport);
}

void ninet_gifconf_hook(void *data, struct net_device *dev,
			char __user *buf, int len, int size, int *ret)
{
	if (*ret >= 0) {
		int done = ninet_gifconf(dev, buf + *ret, len - *ret, size);

		if (done < 0)
			*ret = done;
		else
			*ret += done;
	}
}

int ninet_hooks_register(void)
{
	int ret;

	ret = register_trace_ninet_ehashfn_hook(&ninet_ehashfn_hook, NULL);
	if (ret) {
		nip_dbg("failed to register to ninet_ehashfn_hook");
		return -1;
	}

	ret = register_trace_ninet_gifconf_hook(&ninet_gifconf_hook, NULL);
	if (ret) {
		nip_dbg("failed to register to ninet_gifconf_hook");
		return -1;
	}

	return 0;
}
#endif /* CONFIG_NEWIP_HOOKS */

