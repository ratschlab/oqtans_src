#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/ARPACK
if [ "$1" == "" -o "$1" == "all" ];
then
    make all
    cp libarpack_SUN4.a $OQTANS_DEP_PATH/lib
    cp libarpack_SUN4.a $OQTANS_DEP_PATH/lib/libarpack.a
fi

if [ "$1" == "clean" ];
then
    make clean
fi
