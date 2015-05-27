#!/bin/sh

ROOT="$(pwd)/../"
export XEN_TARGET_ARCH=arm32 
export CROSS_COMPILE=arm-linux-gnueabihf- 

cd $ROOT
make clean -j4 



