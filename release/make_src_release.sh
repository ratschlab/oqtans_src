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
if [ -d oqtans_src ] ;
then
	echo oqtans_src already exists
	exit -1
fi

echo ${reldir}

mkdir -p oqtans_src
cd oqtans_src
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
git tag -a README.git -m "This version only contains README.git" 
git checkout master

git pull --no-edit git://github.com/ratschlab/oqtans_src.git master:refs/remotes/origin/master
git remote add origin git://github.com/ratschlab/oqtans_src.git
git remote add github git@github.com:ratschlab/oqtans_src.git

git checkout README.git

cd ..
tar cvf ${reldir}.tar oqtans_src
rm -f ${reldir}.tar.bz2
bzip2 -v9 ${reldir}.tar
scp ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
echo cleanup
rm -rf oqtans_src
