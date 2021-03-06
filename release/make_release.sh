#!/bin/bash

version=0.1-beta
for platform in master osx-10.7.4 ubuntu-12.04 redhat-6.3 ;
do

	set -e
	
	basedir=`dirname $0`
	if [ "$basedir" == "." ];
	then 
		basedir=`pwd`/
	fi
	
	echo $basedir
	cd $basedir
	
	source ./../../oqtans_config.sh
	
	if [ "${platform}" == "master" ];
	then
		reldir=oqtans-${version}
	else
		reldir=oqtans-${version}-${platform}
	fi
	if [ -d oqtans ] ;
	then
		echo oqtans already exists
		exit -1
	fi
	
	echo ${reldir}
	
	mkdir -p oqtans
	cd oqtans
	git init
	git checkout -b master
	if [ "${platform}" == "master" ];
	then
		echo "This is the source release of Oqtans tools version ${version}" > README.git
	else
		echo "This is the source/binary release of Oqtans tools version ${version} for platform ${platform}" > README.git
	fi
	echo >> README.git
	echo "Type" >> README.git
	echo "   git checkout ${platform}" >> README.git
	if [ "${platform}" == "master" ];
	then
		echo "to get the source release files." >> README.git
	else
		echo "to get the source/binary release for platform ${platform}." >> README.git
	fi
	echo >> README.git 
	echo "Type" >> README.git
	echo "   git fetch origin ${platform}:refs/remotes/origin/${platform}" >> README.git
	if [ "${platform}" == "master" ];
	then
		echo "to obtain source updates." >> README.git
	else
		echo "to obtain updates for platform ${platform}." >> README.git
	fi
	echo >> README.git
	echo "Type" >> README.git
	echo "   git pull origin ${platform}" >> README.git
	if [ "${platform}" == "master" ];
	then
		echo "to download updates for all platforms and to update the source files." >> README.git
	else
		echo "to download updates for all platforms and to update platform ${platform}." >> README.git
	fi
	echo >> README.git
	git add README.git
	git commit -m "added automatically generated README.git file" 
	git tag -a README.git -m "This version only contains README.git" 
	git checkout master

	git pull --no-edit git://github.com/ratschlab/oqtans.git ${platform}:refs/remotes/origin/${platform}
	git remote add origin git://github.com/ratschlab/oqtans.git
	git remote add github git@github.com:ratschlab/oqtans.git

	git checkout README.git

	cd ..
	tar cvf ${reldir}.tar oqtans
	rm -f ${reldir}.tar.bz2
	bzip2 -v9 ${reldir}.tar
	rsync -av ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
	echo cleanup
	rm -rf oqtans
	
done
