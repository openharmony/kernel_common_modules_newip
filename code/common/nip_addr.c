// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
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
#include "nip_addr.h"

/* This is similar to 0.0.0.0 in IPv4. Does not appear as a real address,
 * just a constant used by the native for special processing
 */
const struct nip_addr nip_any_addr = {
	.bitlen = NIP_ADDR_BIT_LEN_16,
	.nip_addr_field8[0] = 0xFF, /* 0xFF09 addr, big-endian */
	.nip_addr_field8[1] = 0x09,
};

const struct nip_addr nip_broadcast_addr_arp = {
	.bitlen = NIP_ADDR_BIT_LEN_16,
	.nip_addr_field8[0] = 0xFF, /* 0xFF04 addr, big-endian */
	.nip_addr_field8[1] = 0x04,
};

/* Short address range:
 * 【1-byte】0 ~ 220
 * 00 ~ DC
 *
 * 【2-byte】221 ~ 5119
 * DD/DE/.../F0 is a 2-byte address descriptor followed by the address value
 * DDDD ~ DDFF : 221 ~ 255
 * DE00 ~ DEFF : 256 ~ 511
 * DF00 ~ DFFF : 512 ~ 767
 * ...
 * F000 ~ F0FF : 4864 ~ 5119
 *
 * 【3-byte】5120 ~ 65535
 * F1 is a 3-byte address descriptor followed by the address value
 * F1 1400 ~ F1 FFFF
 *
 * 【5-byte】65536 ~ 4,294,967,295
 * F2 is a 5-byte address descriptor followed by the address value
 * F2 0001 0000 ~ F2 FFFF FFFF
 *
 * 【7-byte】4,294,967,296 ~ 281,474,976,710,655
 * F3 is a 7-byte address descriptor followed by the address value
 * F3 0001 0000 0000 ~ F3 FFFF FFFF FFFF
 *
 * 【9-byte】281,474,976,710,656 ~ xxxx
 * F4 is a 9-byte address descriptor followed by the address value
 * F4 0001 0000 0000 0000 ~ F4 FFFF FFFF FFFF FFFF
 *
 * 0xFF00 - The loopback address
 * 0xFF01 - Public address for access authentication
 * 0xFF02 - Public address of access authentication
 * 0xFF03 - The neighbor found a public address
 * 0xFF04 - Address resolution (ARP)
 * 0xFF05 - DHCP public address
 * 0xFF06 - Public address for minimalist access authentication
 * 0xFF07 - Self-organizing protocol public address
 * 0xFF08 - The IEEE EUI - 64 addresses
 * 0xFF09 - any_addr
 */
int nip_addr_invalid(const struct nip_addr *addr)
{
	unsigned char first_byte, second_byte, third_byte;
	int addr_len, i, err;

	first_byte = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_0];
	second_byte = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_1];
	third_byte = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_2];
	addr_len = addr->bitlen / NIP_ADDR_BIT_LEN_8;

	/* The value of the field after the effective length of the short address should be 0 */
	for (i = addr_len; i < NIP_8BIT_ADDR_INDEX_MAX; i++) {
		if (addr->nip_addr_field8[i] > 0x00) {
			/* newip bitlen error */
			err = 1;
			return err;
		}
	}

	if (first_byte <= ADDR_FIRST_DC && addr_len == NIP_ADDR_LEN_1) {
		err = 0;
	} else if (first_byte <= ADDR_FIRST_F0 && addr_len == NIP_ADDR_LEN_2) {
		if (first_byte > ADDR_FIRST_DC + 1 ||
		    second_byte >= ADDR_SECOND_MIN_DD) {
			err = 0;
		} else {
			/* addr2 is not valid */
			err = 1;
		}
	} else if (first_byte == ADDR_FIRST_F1 && addr_len == NIP_ADDR_LEN_3) {
		if (second_byte >= ADDR_SECOND_MIN_F1) {
			err = 0;
		} else {
			/* addr3 is not valid */
			err = 1;
		}
	} else if (first_byte == ADDR_FIRST_F2 && addr_len == NIP_ADDR_LEN_5) {
		if (second_byte > 0 || third_byte >= ADDR_THIRD_MIN_F2) {
			err = 0;
		} else {
			/* addr5 is not valid */
			err = 1;
		}
	} else if (first_byte == ADDR_FIRST_FF && addr_len == NIP_ADDR_LEN_2) {
		err = 0;
	} else {
		/* addr check fail */
		err = 1;
	}
	return err;
}

/* 0xFF00 - The loopback address
 * 0xFF01 - Public address for access authentication
 * 0xFF02 - Public address of access authentication
 * 0xFF03 - The neighbor found a public address
 * 0xFF04 - Address resolution (ARP)
 * 0xFF05 - DHCP public address
 * 0xFF06 - Public address for minimalist access authentication
 * 0xFF07 - Self-organizing protocol public address
 * 0xFF08 - The IEEE EUI - 64 addresses
 * 0xFF09 - any_addr
 */
int nip_addr_public(const struct nip_addr *addr)
{
	if (addr->bitlen == NIP_ADDR_BIT_LEN_16 &&
	    addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_0] == ADDR_FIRST_FF)
		return 1;
	else
		return 0;
}

/* judge whether the nip_addr is equal to 0xFF09 */
int nip_addr_any(const struct nip_addr *ad)
{
	int result = 0;

	if (ad->bitlen == NIP_ADDR_BIT_LEN_16) {
		if (ad->nip_addr_field16[0] == nip_any_addr.nip_addr_field16[0] &&
		    ad->nip_addr_field16[1] == nip_any_addr.nip_addr_field16[1])
			result = 1;
	}
	return result;
}

int get_nip_addr_len(const struct nip_addr *addr)
{
	int len = 0;

	if (addr->nip_addr_field8[0] <= ADDR_FIRST_DC)
		len = NIP_ADDR_LEN_1;
	else if ((addr->nip_addr_field8[0] > ADDR_FIRST_DC &&
		  addr->nip_addr_field8[0] <= ADDR_FIRST_F0) ||
		  addr->nip_addr_field8[0] == ADDR_FIRST_FF)
		len = NIP_ADDR_LEN_2;
	else if (addr->nip_addr_field8[0] == ADDR_FIRST_F1)
		len = NIP_ADDR_LEN_3;
	else if (addr->nip_addr_field8[0] == ADDR_FIRST_F2)
		len = NIP_ADDR_LEN_5;
	else
		return 0;
	return len;
}

unsigned char *build_nip_addr(const struct nip_addr *addr, unsigned char *buf)
{
	unsigned char *p = buf;
	int i;

	if (addr->nip_addr_field8[0] <= ADDR_FIRST_DC) {
		*p = addr->nip_addr_field8[0];
	} else if (((addr->nip_addr_field8[0] > ADDR_FIRST_DC) &&
		  (addr->nip_addr_field8[0] <= ADDR_FIRST_F0)) ||
		  (addr->nip_addr_field8[0] == ADDR_FIRST_FF)) {
		*p = addr->nip_addr_field8[0];
		p++;
		*p = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_1];
	} else if (addr->nip_addr_field8[0] == ADDR_FIRST_F1) {
		for (i = 0; i < NIP_ADDR_LEN_2; i++) {
			*p = addr->nip_addr_field8[i];
			p++;
		}
		*p = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_2];
	} else if (addr->nip_addr_field8[0] == ADDR_FIRST_F2) {
		for (i = 0; i < NIP_ADDR_LEN_4; i++) {
			*p = addr->nip_addr_field8[i];
			p++;
		}
		*p = addr->nip_addr_field8[NIP_8BIT_ADDR_INDEX_4];
	} else {
		return 0;
	}

	return ++p;
}

unsigned char *decode_nip_addr(unsigned char *buf, struct nip_addr *addr)
{
	unsigned char *p = buf;
	int i;

	if (*p <= ADDR_FIRST_DC) {
		addr->nip_addr_field8[0] = *p;
		p++;
		addr->bitlen = NIP_ADDR_BIT_LEN_8;
	} else if (*p > ADDR_FIRST_DC && *p <= ADDR_FIRST_F0) {
		if (*p > ADDR_FIRST_DC + 1 || *(p + 1) >= ADDR_SECOND_MIN_DD) {
			addr->nip_addr_field8[0] = *p;
			p++;
			addr->nip_addr_field8[1] = *p;
			p++;
			addr->bitlen = NIP_ADDR_BIT_LEN_16;
		} else {
			return 0;
		}
	} else if (*p == ADDR_FIRST_F1) {
		if (*(p + 1) >= ADDR_SECOND_MIN_F1) {
			for (i = 0; i < NIP_ADDR_LEN_3; i++) {
				addr->nip_addr_field8[i] = *p;
				p++;
			}
			addr->bitlen = NIP_ADDR_BIT_LEN_24;
		} else {
			return 0;
		}
	} else if (*p == ADDR_FIRST_F2) {
		if (*(p + 1) > 0 || *(p + 2) >= ADDR_THIRD_MIN_F2) { /* 偏移2 */
			for (i = 0; i < NIP_ADDR_LEN_5; i++) {
				addr->nip_addr_field8[i] = *p;
				p++;
			}
			addr->bitlen = NIP_ADDR_BIT_LEN_40;
		} else {
			return 0;
		}
	} else if (*p == ADDR_FIRST_FF) {
		addr->nip_addr_field8[0] = *p;
		p++;
		addr->nip_addr_field8[1] = *p;
		p++;
		addr->bitlen = NIP_ADDR_BIT_LEN_16;
	} else {
		return 0;
	}

	return p;
}

