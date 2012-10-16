#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/gperf-3.0.4

if [ "$1" == "" -o "$1" == "all" ];
then
    ./configure --prefix=$OQTANS_DEP_PATH/
    make all
    make install
fi

if [ "$1" == "clean" ];
then
    if [ -f Makefile ];
    then
	make clean
    fi
fi
