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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define AF_NINET_PATH        ("/sys/module/newip/parameters/af_ninet")
#define AF_NINET_LENTH       (5)

int g_af_ninet;

void _get_af_ninet(void)
{
	char tmp[AF_NINET_LENTH];
	FILE *fn = fopen(AF_NINET_PATH, "r");

	if (!fn) {
		printf("fail to open %s.\n\n", AF_NINET_PATH);
		return;
	}

	if (fgets(tmp, AF_NINET_LENTH, fn) == NULL) {
		printf("fail to gets %s.\n\n", AF_NINET_PATH);
		fclose(fn);
		return;
	}

	fclose(fn);
	g_af_ninet = atoi(tmp);
}

int get_af_ninet(void)
{
	if (g_af_ninet == 0)
		_get_af_ninet();

	return g_af_ninet;
}

int main(int argc, char **argv)
{
	int af_ninet = get_af_ninet();

	printf("af_ninet=%d\n\n", g_af_ninet);
	return 0;
}

