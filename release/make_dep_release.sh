#!/bin/bash


set -e

version=0.1-beta
for platform in ubuntu-12.04 ubuntu-10.04 redhat-6.3 osx-10.7.4 ;
do
	echo platform $platform
	
	basedir=`dirname $0`
	if [ "$basedir" == "." ];
	then 
		basedir=`pwd`/
	fi
	
	echo $basedir
	cd $basedir
	
	source ../oqtans_conf.sh
	
	reldir=oqtans_dep-${version}-${platform}
	if [ -d oqtans_dep ] ;
	then
		echo oqtans_dep already exists
		exit -1
	fi
	
	echo ${reldir}
	
	mkdir -p oqtans_dep
	cd oqtans_dep
	git init
	git checkout -b master
	echo "This is the binary release of Oqtans dependencies version ${version}" > README.git
	echo >> README.git
	echo "Type" >> README.git
	echo "   git checkout ${platform}" >> README.git
	echo "to get the binary release for platform ${platform}." >> README.git
	echo >> README.git 
	echo "Type" >> README.git
	echo "   git fetch origin ${platform}:refs/remotes/origin/${platform}" >> README.git
	echo "to obtain updates for platform ${platform}." >> README.git
	echo >> README.git
	echo "Type" >> README.git
	echo "   git pull origin ${platform}" >> README.git
	echo "to download updates for all platforms and to update platform ${platform}." >> README.git
	echo >> README.git
	git add README.git
	git commit -m "added automatically generated README.git file" 
	git fetch git://github.com/ratschlab/oqtans_dep.git ${platform}:refs/remotes/origin/${platform}
	git remote add origin git://github.com/ratschlab/oqtans_dep.git
	git remote add github git@github.com:ratschlab/oqtans_dep.git
	cd ..
	tar cvf ${reldir}.tar oqtans_dep
	rm -f ${reldir}.tar.bz2
	bzip2 -v9 ${reldir}.tar
	rsync -av ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
	echo cleanup
	rm -rf oqtans_dep

done
