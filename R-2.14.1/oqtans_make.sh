#!/bin/bash
set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/R-2.14.1

if [ "$1" == "" -o "$1" == "all" ];
then
    ./configure --prefix=$OQTANS_DEP_PATH/R-2.14.1 --with-x=no
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
