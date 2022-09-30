/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _NIP_CHECKSUM_H
#define _NIP_CHECKSUM_H

#include "nip_addr.h"

struct nip_pseudo_header {
	struct nip_addr saddr;    /* Source address, network order.(big end) */
	struct nip_addr daddr;    /* Destination address, network order.(big end) */
	unsigned short check_len; /* network order.(big end) */
	unsigned char nexthdr;    /* Upper-layer Protocol Type: IPPROTO_UDP */
};

/* The checksum is calculated when the packet is received
 * Note:
 * 1.chksum_header->check_len is network order.(big end)
 * 2.data_len is host order.
 */
unsigned short nip_check_sum_parse(unsigned char *data,
				   unsigned short check_len,
				   struct nip_pseudo_header *chksum_header);

/* The checksum is calculated when the packet is sent
 * Note:
 * 1.chksum_header->check_len is network order.(big end)
 * 2.data_len is host order.
 */
unsigned short nip_check_sum_build(unsigned char *data,
				   unsigned short data_len,
				   struct nip_pseudo_header *chksum_header);

#endif /* _NIP_CHECKSUM_H */

