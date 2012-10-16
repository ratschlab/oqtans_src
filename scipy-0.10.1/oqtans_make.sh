#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/scipy-0.10.1

if [ "$1" == "" -o "$1" == "all" ];
then
    PYTHONPATH=$OQTANS_DEP_PATH/lib/python${OQTANS_PYTHON_VERSION}/site-packages:$OQTANS_DEP_PATH/lib64/python${OQTANS_PYTHON_VERSION}/site-packages LAPACK=$OQTANS_DEP_PATH/lib BLAS=$OQTANS_DEP_PATH/lib AMD=$OQTANS_DEP_PATH UMFPACK=$OQTANS_DEP_PATH python${OQTANS_PYTHON_VERSION} setup.py install --prefix=$OQTANS_DEP_PATH
fi

if [ "$1" == "clean" ];
then
    rm -rf build
fi
