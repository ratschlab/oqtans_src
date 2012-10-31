#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/octave-3.4.3_64

if [ "$1" == "" -o "$1" == "all" ];
then
    opts="--enable-64" # --disable-readline
    if [ `uname` == "Darwin" ];
    then
	opts="--without-framework-carbon --enable-64"
    fi
    export LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib
    ./configure LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib CPPFLAGS="-I$OQTANS_DEP_PATH/include" LIBS="-L$OQTANS_DEP_PATH/lib/ -L/opt/local/lib" LDFLAGS="-L$OQTANS_DEP_PATH/lib/ -L/opt/local/lib" F77=gfortran --prefix=$OQTANS_DEP_PATH/octave-3.4.3  --with-glpk-includedir=$OQTANS_DEP_PATH/include --with-glpk-libdir=$OQTANS_DEP_PATH/lib  --with-qhull-includedir=$OQTANS_DEP_PATH/include/ --with-qhull-libdir=$OQTANS_DEP_PATH/lib --with-qrupdate-libdir=$OQTANS_DEP_PATH/lib/ --with-qrupdate-includedir=$OQTANS_DEP_PATH/include/ $opts --with-umfpack="-lumfpack -lsuitesparseconfig"  --with-umfpack-libdir=$OQTANS_DEP_PATH/lib/ --with-umfpack-includedir=$OQTANS_DEP_PATH/include/
#--without-umfpack 
    
    LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib make -j 1
    
    LD_LIBRARY_PATH=$OQTANS_DEP_PATH/lib make -j 1 install
fi

if [ "$1" == "clean" ];
then
	if [ -f Makefile ];
	then
		make clean
		make distclean
	fi
fi
