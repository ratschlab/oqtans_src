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
	echo "This is the binary release of Oqtans dependencies version ${version}" > README
	echo >> README
	echo "Type" >> README
	echo "   git checkout ${platform}" >> README
	echo "to get the binary release for platform ${platform}." >> README
	echo >> README 
	echo "Type" >> README
	echo "   git fetch origin ${platform}:refs/remotes/origin/${platform}" >> README
	echo "to obtain updates for platform ${platform}." >> README
	echo >> README
	echo "Type" >> README
	echo "   git pull origin ${platform}" >> README
	echo "to download updates for all platforms and to update platform ${platform}." >> README
	echo >> README
	git add README
	git commit -m "added automatically generated README file" 
	git fetch git://github.com/ratschlab/oqtans_dep.git ${platform}:refs/remotes/origin/${platform}
	git remote add origin git://github.com/ratschlab/oqtans_dep.git
	git remote add github git@github.com:ratschlab/oqtans_dep.git
	cd ..
	tar cvf ${reldir}.tar ${reldir}
	rm -f ${reldir}.tar.bz2
	bzip2 -v9 ${reldir}.tar
	rsync -av ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
	echo cleanup
	rm -rf ${reldir}

done
