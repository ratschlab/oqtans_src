#!/bin/bash

set -e 

source ../oqtans_conf.sh

cd $OQTANS_SRC_PATH/samtools
if [ "$1" == "" -o "$1" == "all" ];
then
    make all
    cp -a samtools $OQTANS_DEP_PATH/bin/
    cp -a libbam.a $OQTANS_DEP_PATH/lib/
    cp -a sam.h bam.h bgzf.h $OQTANS_DEP_PATH/include/
fi

if [ "$1" == "clean" ];
then
    make clean
fi
