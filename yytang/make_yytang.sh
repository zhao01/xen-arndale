#!/bin/sh

ROOT="$(pwd)/../"
export CROSS_COMPILE=arm-linux-gnueabihf- 
export XEN_TARGET_ARCH=arm32 

cd $ROOT

make dist-xen
mkimage -A arm -T kernel -a 0x80200000 -e 0x80200000 -C none -d "./xen/xen" "./xen/xen-uImage"

