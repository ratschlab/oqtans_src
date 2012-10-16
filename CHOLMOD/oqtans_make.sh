#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/CHOLMOD
if [ "$1" == "" -o "$1" == "all" ];
then
    cd Lib
    make all
    cd ..
    make install
fi

if [ "$1" == "clean" ];
then
    make clean
fi
