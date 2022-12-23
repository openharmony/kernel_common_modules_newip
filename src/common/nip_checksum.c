// SPDX-License-Identifier: BSD-3-Clause
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
#include "nip_hdr.h"
#include "nip_checksum.h"

#define USHORT_PAYLOAD 16
#define NIP_CHECKSUM_UINT8_PAYLOAD 8
unsigned int _nip_check_sum(const unsigned char *data, unsigned short data_len)
{
	unsigned int i = 0;
	unsigned int sum = 0;

	while (i + 1 < data_len) {
		sum += (data[i] << NIP_CHECKSUM_UINT8_PAYLOAD) + data[i + 1];
		i += 2; /* Offset 2 bytes */
	}

	if (i < (unsigned int)data_len)
		sum += (data[i] << NIP_CHECKSUM_UINT8_PAYLOAD);

	return sum;
}

unsigned int _nip_header_chksum(struct nip_pseudo_header *chksum_header)
{
	int i, j;
	int addr_len;
	unsigned char pseudo_header[NIP_HDR_MAX] = {0};
	unsigned short hdr_len = 0;

	addr_len = chksum_header->saddr.bitlen / NIP_ADDR_BIT_LEN_8;
	if (addr_len) {
		j = 0;
		for (i = 0; i < addr_len; i++, j++)
			pseudo_header[j] = chksum_header->saddr.nip_addr_field8[i];
		hdr_len += addr_len;
	}

	addr_len = chksum_header->daddr.bitlen / NIP_ADDR_BIT_LEN_8;
	if (addr_len) {
		j = hdr_len;
		for (i = 0; i < addr_len; i++, j++)
			pseudo_header[j] = chksum_header->daddr.nip_addr_field8[i];
		hdr_len += addr_len;
	}

	/* chksum_header->check_len is network order.(big end) */
	*(unsigned short *)(pseudo_header + hdr_len) = chksum_header->check_len;
	hdr_len += sizeof(chksum_header->check_len);
	*(pseudo_header + hdr_len) = chksum_header->nexthdr;
	hdr_len += sizeof(chksum_header->nexthdr);

	return _nip_check_sum(pseudo_header, hdr_len);
}

/* The checksum is calculated when the packet is received
 * Note:
 * 1.chksum_header->check_len is network order.(big end)
 * 2.check_len is host order.
 */
unsigned short nip_check_sum_parse(unsigned char *data,
				   unsigned short check_len,
				   struct nip_pseudo_header *chksum_header)
{
	unsigned int sum = 0;

	sum = _nip_check_sum(data, check_len);
	sum += _nip_header_chksum(chksum_header);

	while (sum >> USHORT_PAYLOAD)
		sum = (sum >> USHORT_PAYLOAD) + (sum & 0xffff);
	return (unsigned short)sum;
}

/* The checksum is calculated when the packet is sent
 * Note:
 * 1.chksum_header->check_len is network order.(big end)
 * 2.data_len is host order.
 */
unsigned short nip_check_sum_build(unsigned char *data,
				   unsigned short data_len,
				   struct nip_pseudo_header *chksum_header)
{
	unsigned int sum = 0;

	sum = _nip_check_sum(data, data_len);
	sum += _nip_header_chksum(chksum_header);

	while (sum >> USHORT_PAYLOAD)
		sum = (sum >> USHORT_PAYLOAD) + (sum & 0xffff);
	return (unsigned short)(~sum);
}

