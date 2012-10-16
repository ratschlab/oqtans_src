#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/swig-2.0.8

if [ "$1" == "" -o "$1" == "all" ];
then
    ./configure --prefix=$OQTANS_DEP_PATH
    
    make -j 5
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
