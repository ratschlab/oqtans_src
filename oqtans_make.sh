#!/bin/bash

set -e

basedir=`dirname $0`
if [ "$basedir" == "." ];
then 
    basedir=`pwd`/
fi

echo $basedir

packages="flex-2.5.37 gperf-3.0.4 BLAS lapack-3.4.1 ARPACK qrupdate-1.1.2 SuiteSparse glpk-4.47 "
if [ `uname` != "Darwin" ];
then
	packages="$packages CHOLMOD qhull scipy-0.10.1 numpy-1.6.2 R-2.14.1"
fi
packages="$packages octave-3.4.3_64 swig-2.0.8 shogun-1.1.0"

#packages="cmake-2.8.9 flex-2.5.37 $packages"

for i in  $packages ;
do
    cd $basedir/$i
    echo ==============================================================
    echo ===================== making $i ===============
    echo ==============================================================
    ./oqtans_make.sh $1
done

