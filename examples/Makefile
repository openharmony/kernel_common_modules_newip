# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (c) 2022 Huawei Device Co., Ltd.
#
# Makefile for the Linux newip layer
#
CC=gcc
# CC = aarch64-linux-gnu-gcc
# CC = arm-linux-gnueabi-gcc
CFLAGS=-pthread -static -g

UT_LIST = nip_addr_cfg_demo nip_route_cfg_demo nip_tcp_server_demo nip_tcp_client_demo nip_udp_server_demo nip_udp_client_demo

all: $(UT_LIST)

clean:
	rm -f $(UT_LIST)
	rm -f nip_api.o
	rm -f libnip_api.a


#lib
NIP_LIB     = libnip_api.a
NIP_DEF_LIB = -L. -lnip_api

nip_api.o: nip_api.c
	$(CC) -c nip_api.c -o nip_api.o

libnip_api.a: nip_api.o
	ar -rv libnip_api.a nip_api.o

#UT func list
nip_addr_cfg_demo: nip_addr_cfg_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_addr_cfg_demo nip_addr_cfg_demo.c $(NIP_DEF_LIB)

nip_route_cfg_demo: nip_route_cfg_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_route_cfg_demo nip_route_cfg_demo.c $(NIP_DEF_LIB)

nip_tcp_server_demo: nip_tcp_server_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_tcp_server_demo nip_tcp_server_demo.c $(NIP_DEF_LIB)

nip_tcp_client_demo: nip_tcp_client_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_tcp_client_demo nip_tcp_client_demo.c $(NIP_DEF_LIB)

nip_udp_server_demo: nip_udp_server_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_udp_server_demo nip_udp_server_demo.c $(NIP_DEF_LIB)

nip_udp_client_demo: nip_udp_client_demo.c $(NIP_LIB)
	$(CC) $(CFLAGS) -o nip_udp_client_demo nip_udp_client_demo.c $(NIP_DEF_LIB)
