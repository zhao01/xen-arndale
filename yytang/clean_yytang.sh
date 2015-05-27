#!/bin/sh

ROOT="$(pwd)/../"
export CROSS_COMPILE=arm-linux-gnueabihf- 
export XEN_TARGET_ARCH=arm32 

cd $ROOT

make clean 

