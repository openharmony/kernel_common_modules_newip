// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Definitions for the NewIP Hooks Register module.
 */
#ifdef CONFIG_NEWIP_HOOKS
#define pr_fmt(fmt) "NIP: " fmt

#include <net/ninet_hashtables.h>      /* ninet_ehashfn */
#include <net/if_ninet.h>
#include <trace/hooks/nip_hooks.h>

void ninet_ehashfn_hook(void *data, const struct net *net,
			const struct nip_addr *laddr, const u16 lport,
			const struct nip_addr *faddr, const __be16 fport, u32 *ret)
{
	*ret = ninet_ehashfn(net, laddr, lport, faddr, fport);
}

void ninet_gifconf_hook(void *data, struct net_device *dev,
			char __user *buf, int len, int size, int *ret)
{
	*ret = ninet_gifconf(dev, buf, len, size);
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

