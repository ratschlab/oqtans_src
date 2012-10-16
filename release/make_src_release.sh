#!/bin/bash

version=0.1-beta

set -e

basedir=`dirname $0`
if [ "$basedir" == "." ];
then 
    basedir=`pwd`/
fi

echo $basedir
cd $basedir

source ../oqtans_conf.sh

reldir=oqtans_src-${version}
if [ -d ${reldir} ] ;
then
	echo ${reldir} already exists
	exit -1
fi

echo ${reldir}

mkdir -p ${reldir}
cd ${reldir}
git init
git checkout -b master
echo "This is the source release of Oqtans dependencies version ${version}" > README.git
echo >> README.git
echo "Type" >> README.git
echo "   git checkout master" >> README.git
echo "to get the source release." >> README.git
echo >> README.git 
echo "Type" >> README.git
echo "   git pull origin master" >> README.git
echo "to obtain updates of the source release." >> README.git
echo >> README.git
git add README.git
git commit -m "added automatically generated README.git file" 
git fetch git://github.com/ratschlab/oqtans_src.git master:refs/remotes/origin/master
git remote add origin git://github.com/ratschlab/oqtans_src.git
git remote add github git@github.com:ratschlab/oqtans_src.git
cd ..
tar cvf ${reldir}.tar ${reldir}
bzip2 -v9 ${reldir}.tar
scp ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
echo cleanup
rm -rf ${reldir}
