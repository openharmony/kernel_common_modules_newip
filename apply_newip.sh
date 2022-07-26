#!/bin/bash
#
# Copyright (c) 2022 Huawei Device Co., Ltd.
#
# NewIP is dual licensed: you can use it either under the terms of
# the GPL, or the BSD license, at your option.
# See the LICENSE file in directory / of this repository for complete details.
#

set -e

OHOS_SOURCE_ROOT=$1
KERNEL_BUILD_ROOT=$2
PRODUCT_NAME=$3
KERNEL_VERSION=$4

PATCH_FILE=$OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/patches/$KERNEL_VERSION/newip.patch
PRODUCT_SWITCH=$OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/patches/$PRODUCT_NAME.flag
function main()
{
	if [ ! -f $PATCH_FILE ]; then
		echo "newip not supportted!kernel=$KERNEL_VERSION!"
		return;
	fi
	if [ ! -f $PRODUCT_SWITCH ]; then
		echo "newip not supportted!product=$PRODUCT_NAME!"
		return;
	fi
	

    cd $KERNEL_BUILD_ROOT
    echo "patch for newip..."
    patch -p1 < $PATCH_FILE

	cp -R $OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/code/linux/net/newip  net/
    cp -arfL $OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/code/linux/include/*  include/

	cp -arfL $OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/code/common/*.h  net/newip/
	cp -arfL $OHOS_SOURCE_ROOT/foundation/communication/sfc/newip/code/common/*.c  net/newip/
	ln -s -f $KERNEL_BUILD_ROOT/net/newip/nip_addr.h $KERNEL_BUILD_ROOT/include/uapi/linux/nip_addr.h

    cd -
}

main
