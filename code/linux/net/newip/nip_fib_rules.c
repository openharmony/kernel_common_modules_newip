// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP Routing Policy Rules
 *
 * Based on net/ipv6/fib_rules.c
 * Based on net/ipv6/fib6_rules.c
 */
#include <net/nip_fib.h>
#include <linux/netdevice.h>
#include <linux/notifier.h>
#include <linux/export.h>

struct dst_entry *nip_fib_rule_lookup(struct net *net, struct flow_nip *fln,
				      int flags, nip_pol_lookup_t lookup)
{
	struct nip_rt_info *rt;

	rt = lookup(net, net->newip.nip_fib_local_tbl, fln, flags);
	if (rt != net->newip.nip_null_entry)
		return &rt->dst;
	nip_rt_put(rt);
	rt = lookup(net, net->newip.nip_fib_main_tbl, fln, flags);
	if (rt != net->newip.nip_null_entry)
		return &rt->dst;
	nip_rt_put(rt);

	dst_hold(&net->newip.nip_null_entry->dst);
	return &net->newip.nip_null_entry->dst;
}
