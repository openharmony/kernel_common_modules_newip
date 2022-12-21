// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * NewIP Hooks
 */
#define CREATE_TRACE_POINTS
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/nip_hooks.h>

#ifdef CONFIG_NEWIP_HOOKS
EXPORT_TRACEPOINT_SYMBOL_GPL(ninet_ehashfn_hook);
EXPORT_TRACEPOINT_SYMBOL_GPL(ninet_gifconf_hook);
#endif
