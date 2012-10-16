#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/cmake-2.8.9
if [ "$1" == "" -o "$1" == "all" ];
then
    ./configure --prefix=$OQTANS_DEP_PATH/ --no-system-libs
    make all
    make install
fi

if [ "$1" == "clean" ];
then
    make clean
fi
