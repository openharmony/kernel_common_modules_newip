#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Huawei Device Co., Ltd.
#

set -e

OHOS_SOURCE_ROOT=$1
KERNEL_BUILD_ROOT=$2
PRODUCT_NAME=$3
KERNEL_VERSION=$4
NEWIP_SOURCE_ROOT=$OHOS_SOURCE_ROOT/kernel/common_modules/newip

function main()
{
	cd $KERNEL_BUILD_ROOT

	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/include/linux/*.h            $KERNEL_BUILD_ROOT/include/linux/
	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/include/net/netns/*.h        $KERNEL_BUILD_ROOT/include/net/netns/
	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/include/net/*.h              $KERNEL_BUILD_ROOT/include/net/
	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/include/uapi/linux/*.h       $KERNEL_BUILD_ROOT/include/uapi/linux/
	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/include/trace/hooks/*.h      $KERNEL_BUILD_ROOT/include/trace/hooks/
	
	if [ ! -d " $KERNEL_BUILD_ROOT/net/newip" ]; then
		mkdir $KERNEL_BUILD_ROOT/net/newip
	fi
	ln -s -f $NEWIP_SOURCE_ROOT/src/linux/net/newip/*                  $KERNEL_BUILD_ROOT/net/newip/
	ln -s -f $NEWIP_SOURCE_ROOT/src/common/*                           $KERNEL_BUILD_ROOT/net/newip/
	ln -s -f $NEWIP_SOURCE_ROOT/src/common/nip_addr.h                  $KERNEL_BUILD_ROOT/include/uapi/linux/nip_addr.h
	
	cd -
}

main
