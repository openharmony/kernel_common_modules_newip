#!/bin/bash
#
# Copyright (c) 2022 Huawei Device Co., Ltd.
#
# you can use it under the terms of the GPL V2 and the BSD2 license.
# See the LICENSE file in directory / of this repository for complete details.
#

set -e

OHOS_SOURCE_ROOT=$1
KERNEL_BUILD_ROOT=$2
PRODUCT_NAME=$3
KERNEL_VERSION=$4
NEWIP_SOURCE_ROOT=$OHOS_SOURCE_ROOT/foundation/communication/sfc/newip

function main()
{
	cd $KERNEL_BUILD_ROOT

	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/linux/newip_route.h            $KERNEL_BUILD_ROOT/include/linux/newip_route.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/linux/nip.h                    $KERNEL_BUILD_ROOT/include/linux/nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/linux/nip_icmp.h               $KERNEL_BUILD_ROOT/include/linux/nip_icmp.h
	
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/netns/nip.h                $KERNEL_BUILD_ROOT/include/net/netns/nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/flow_nip.h                 $KERNEL_BUILD_ROOT/include/net/flow_nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/if_ninet.h                 $KERNEL_BUILD_ROOT/include/net/if_ninet.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/ninet_connection_sock.h    $KERNEL_BUILD_ROOT/include/net/ninet_connection_sock.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/ninet_hashtables.h         $KERNEL_BUILD_ROOT/include/net/ninet_hashtables.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nip.h                      $KERNEL_BUILD_ROOT/include/net/nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nip_addrconf.h             $KERNEL_BUILD_ROOT/include/net/nip_addrconf.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nip_fib.h                  $KERNEL_BUILD_ROOT/include/net/nip_fib.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nip_route.h                $KERNEL_BUILD_ROOT/include/net/nip_route.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nip_udp.h                  $KERNEL_BUILD_ROOT/include/net/nip_udp.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/nndisc.h                   $KERNEL_BUILD_ROOT/include/net/nndisc.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/tcp_nip.h                  $KERNEL_BUILD_ROOT/include/net/tcp_nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/net/transp_nip.h               $KERNEL_BUILD_ROOT/include/net/transp_nip.h
	
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/uapi/linux/newip_route.h       $KERNEL_BUILD_ROOT/include/uapi/linux/newip_route.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/uapi/linux/nip.h               $KERNEL_BUILD_ROOT/include/uapi/linux/nip.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/uapi/linux/nip_icmp.h          $KERNEL_BUILD_ROOT/include/uapi/linux/nip_icmp.h
	
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/include/trace/hooks/nip_hooks.h        $KERNEL_BUILD_ROOT/include/trace/hooks/nip_hooks.h
	
	ln -s -f $NEWIP_SOURCE_ROOT/code/linux/net/newip                              $KERNEL_BUILD_ROOT/net
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_addr.c                            $KERNEL_BUILD_ROOT/net/newip/nip_addr.c
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_addr.h                            $KERNEL_BUILD_ROOT/net/newip/nip_addr.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_addr.h                            $KERNEL_BUILD_ROOT/include/uapi/linux/nip_addr.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_checksum.c                        $KERNEL_BUILD_ROOT/net/newip/nip_checksum.c
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_checksum.h                        $KERNEL_BUILD_ROOT/net/newip/nip_checksum.h
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_hdr_decap.c                       $KERNEL_BUILD_ROOT/net/newip/nip_hdr_decap.c
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_hdr_encap.c                       $KERNEL_BUILD_ROOT/net/newip/nip_hdr_encap.c
	ln -s -f $NEWIP_SOURCE_ROOT/code/common/nip_hdr.h                             $KERNEL_BUILD_ROOT/net/newip/nip_hdr.h

	cd -
}

main
