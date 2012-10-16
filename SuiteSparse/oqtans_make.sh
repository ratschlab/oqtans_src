#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/SuiteSparse

if [ "$1" == "" -o "$1" == "all" ];
then
    export prefix64=$OQTANS_SRC_PATH/SuiteSparse
    make
    make install
fi

if [ "$1" == "clean" ];
then
    if [ -f Makefile ];
    then
		make clean
		make distclean
    fi
fi
