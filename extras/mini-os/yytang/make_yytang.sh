#!/bin/sh

ROOT="$(pwd)/../"
export XEN_TARGET_ARCH=arm32 
export CROSS_COMPILE=arm-linux-gnueabihf- 

cd $ROOT
make LWIPDIR=lwip-1.3.2 -j4 
#make -j4


