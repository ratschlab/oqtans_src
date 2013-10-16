#!/bin/bash
#
# compile all dependency modules required for oqtans 
#
set -e

basedir=`dirname $0`
if [ "$basedir" == "." ];
then
    basedir=`pwd`/
fi

echo $basedir

#
# to make libraries
#
if [ "$1" == "" -o "$1" == "all" ];
then 
    for fn in cmake-2.8.11.2.tar.bz2 flex-2.5.37.tar.bz2 gperf-3.0.4.tar.bz2 lapack-3.4.2.tar.bz2 ARPACK.tar.bz2 qrupdate-1.1.2.tar.bz2 SuiteSparse-4.2.1.tar.bz2 qhull.tar.bz2 glpk-4.51.tar.bz2;
    do
        tar -xjf $fn
        cd $basedir/${fn%.tar.bz2}
        echo ==============================================================
        echo        making ${fn%.tar.bz2} 
        echo ==============================================================
        bash oqtans_make.sh $1
        echo ==============================================================
        echo        done ${fn%.tar.bz2} 
        echo ==============================================================
        cd ..
    done

    export BLAS=${OQTANS_DEP_PATH}/lib
    export LAPACK=${OQTANS_DEP_PATH}/lib

    if [ -d ${OQTANS_DEP_PATH}/lib/python${OQTANS_PYTHON_VERSION}/site-packages ]
    then       
	    echo "Using PYTHONPATH prefix as: ${OQTANS_DEP_PATH}/lib/python${OQTANS_PYTHON_VERSION}/site-packages"
    else
	    mkdir -p ${OQTANS_DEP_PATH}/lib/python${OQTANS_PYTHON_VERSION}/site-packages ## create PYTHONPATH dir
	    mkdir -p ${OQTANS_DEP_PATH}/lib64/python${OQTANS_PYTHON_VERSION}/site-packages
    fi 

    easy_install --prefix=${OQTANS_DEP_PATH} numpy
    easy_install --prefix=${OQTANS_DEP_PATH} scipy 
    easy_install --prefix=${OQTANS_DEP_PATH} biopython
    easy_install --prefix=${OQTANS_DEP_PATH} matplotlib
    easy_install --prefix=${OQTANS_DEP_PATH} HTSeq
    easy_install --prefix=${OQTANS_DEP_PATH} rpy2

    for fn in antlr_python_runtime-3.1.3.tar.bz2 arff-1.0c.tar.bz2;
    do 
        tar -xjf $fn
        cd $basedir/${fn%.tar.bz2}
        echo ==============================================================
        echo        making pymodule ${fn%.tar.bz2} 
        echo ==============================================================
        bash oqtans_make.sh $1
        echo ==============================================================
        echo        done ${fn%.tar.bz2} 
        echo ==============================================================
        cd ..
    done 

    for fn in octave-3.6.4_x64.tar.bz2 swig-2.0.11.tar.bz2 shogun-2.0.0.tar.bz2 samtools-0.1.19.tar.bz2 STAR_2.3.0e.Linux_x86_64.tar.bz2;
    do 
        tar -xjf $fn
        cd $basedir/${fn%.tar.bz2}
        echo ==============================================================
        echo        making ${fn%.tar.bz2} 
        echo ==============================================================
        bash oqtans_make.sh $1
        echo ==============================================================
        echo        done ${fn%.tar.bz2} 
        echo ==============================================================
        cd .. 
    done
fi
#
# to clean the compiled dirs
#
if [ "$1" == "clean" ];
then
    for fn in cmake-2.8.11.2 flex-2.5.37 gperf-3.0.4 lapack-3.4.2 ARPACK qrupdate-1.1.2 SuiteSparse-4.2.1 qhull glpk-4.51 octave-3.6.4_x64 swig-2.0.11 shogun-2.0.0; 
    do
        cd $basedir/${fn%.tar.bz2}
        echo ==============================================================
        echo        cleaning ${fn%.tar.bz2} 
        echo ==============================================================
        bash oqtans_make.sh $1
        echo ==============================================================
        echo        done ${fn%.tar.bz2} 
        echo ==============================================================
        cd .. 
    done 
fi
