#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/lapack-3.4.1
if [ "$1" == "" -o "$1" == "all" ];
then
    make
    cp liblapack.a libtmglib.a $OQTANS_DEP_PATH/lib
fi

if [ "$1" == "clean" ];
then
    make clean
fi
