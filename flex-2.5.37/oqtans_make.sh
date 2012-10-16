#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/flex-2.5.37
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
	make distclean 
    fi
fi
