#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/BLAS

if [ "$1" == "" -o "$1" == "all" ];
then
    make
    cp blas_LINUX.a $OQTANS_DEP_PATH/lib
    cp blas_LINUX.a $OQTANS_DEP_PATH/lib/libblas.a
fi

if [ "$1" == "clean" ];
then
    make clean
fi
