/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NewIP Hooks
 *
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 */
#ifdef CONFIG_NEWIP_HOOKS

/* include/trace/hooks/nip_hooks.h */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM       nip_hooks   /* Header file name */
#define TRACE_INCLUDE_PATH trace/hooks /* Header file directory */

/* Header files prevent duplicate definitions */
#if !defined(_TRACE_HOOK_NIP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_NIP_H
#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

struct net;
struct nip_addr;
DECLARE_HOOK(ninet_ehashfn_hook,
      TP_PROTO(const struct net *net, const struct nip_addr *laddr, const u16 lport,
               const struct nip_addr *faddr, const __be16 fport, u32 *ret),
      TP_ARGS(net, laddr, lport, faddr, fport, ret)
);
#endif /* end of _TRACE_HOOK_NIP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

#endif /* end of CONFIG_NEWIP_HOOKS */