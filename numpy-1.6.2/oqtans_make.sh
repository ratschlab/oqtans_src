#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/numpy-1.6.2

if [ "$1" == "" -o "$1" == "all" ];
then
    python${OQTANS_PYTHON_VERSION} setup.py install --prefix=$OQTANS_DEP_PATH
fi


if [ "$1" == "clean" ];
then
    rm -rf build
fi
