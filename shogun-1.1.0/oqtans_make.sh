#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/shogun-1.1.0/src

if [ "$1" == "" -o "$1" == "all" ];
then
    PATH=$OQTANS_DEP_PATH/bin:$OQTANS_DEP_PATH/octave/bin:$PATH LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib ./configure --prefix=$OQTANS_DEP_PATH --interfaces=octave_static,python_modular #--cxxflags="-I$OQTANS_DEP_PATH/ATLAS-3.10.0/include" --cflags="-I$OQTANS_DEP_PATH/ATLAS-3.10.0/include" --ldflags="-L$OQTANS_DEP_PATH/ATLAS-3.10.0/lib"
    
    make -j 4
    make install
    #cp interfaces/octave_static/sg.oct $OQTANS_DEP_PATH/utils/
fi

if [ "$1" == "clean" ];
then
    if [ -f Makefile ];
    then
	make clean
	make distclean
    fi
fi
