#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/glpk-4.47

if [ "$1" == "" -o "$1" == "all" ];
then
    export LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib
    
    ./configure --enable-dl --disable-odbc --disable-mysql --enable-dependency-tracking --enable-shared LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib CPPFLAGS="-I$OQTANS_DEP_PATH/include -fPIC -O -DLP64" LIBS="-L$OQTANS_DEP_PATH/lib" LDFLAGS="-L$OQTANS_DEP_PATH/lib" F77=gfortran --prefix=$OQTANS_DEP_PATH/glpk-4.47_64/  CFLAGS="-fPIC -O -DLP64 "  --prefix=$OQTANS_DEP_PATH
    
    make -j 10
    make install
fi

if [ "$1" == "clean" ];
then
    if [ -f Makefile ];
    then
        make distclean
    fi
fi
