#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/qrupdate-1.1.2

if [ "$1" == "" -o "$1" == "all" ];
then
    make solib
    rm -f $OQTANS_DEP_PATH/lib/libqrupdate.*
    make install
    rsync -av lib/libqrupdate.* $OQTANS_DEP_PATH/lib/
fi

if [ "$1" == "clean" ];
then
    make clean
fi
