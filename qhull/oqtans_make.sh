#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/qhull
if [ "$1" == "" -o "$1" == "all" ];
then
    make all
    make install
fi

if [ "$1" == "clean" ];
then
    make clean
fi
