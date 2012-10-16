#!/bin/bash

version=0.1-beta
for platform in master osx-10.7.4 ubuntu-12.04 ubuntu-10.04 redhat-6.3 ;
do

	set -e
	
	basedir=`dirname $0`
	if [ "$basedir" == "." ];
	then 
		basedir=`pwd`/
	fi
	
	echo $basedir
	cd $basedir
	
	source ../oqtans_conf.sh
	
	if [ "${platform}" == "master" ];
	then
		reldir=oqtans_dep-${version}
	else
		reldir=oqtans_dep-${version}-${platform}
	fi
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
	if [ "${platform}" == "master" ];
	then
		echo "This is the source release of Oqtans tools version ${version}" > README
	else
		echo "This is the source/binary release of Oqtans tools version ${version} for platform ${platform}" > README
	fi
	echo >> README
	echo "Type" >> README
	echo "   git checkout ${platform}" >> README
	if [ "${platform}" == "master" ];
	then
		echo "to get the source release files." >> README
	else
		echo "to get the source/binary release for platform ${platform}." >> README
	fi
	echo >> README 
	echo "Type" >> README
	echo "   git fetch origin ${platform}:refs/remotes/origin/${platform}" >> README
	if [ "${platform}" == "master" ];
	then
		echo "to obtain source updates." >> README
	else
		echo "to obtain updates for platform ${platform}." >> README
	fi
	echo >> README
	echo "Type" >> README
	echo "   git pull origin ${platform}" >> README
	if [ "${platform}" == "master" ];
	then
		echo "to download updates for all platforms and to update the source files." >> README
	else
		echo "to download updates for all platforms and to update platform ${platform}." >> README
	fi
	echo >> README
	git add README
	git commit -m "added automatically generated README file" 
	git fetch git://github.com/ratschlab/oqtans.git ${platform}:refs/remotes/origin/${platform}
	git remote add origin git://github.com/ratschlab/oqtans.git
	git remote add github git@github.com:ratschlab/oqtans.git
	cd ..
	tar cvf ${reldir}.tar ${reldir}
	rm -f ${reldir}.tar.bz2
	bzip2 -v9 ${reldir}.tar
	rsync -av ${reldir}.tar.bz2 raetsch@cbio:~/raetschlab/software/oqtans/
	echo cleanup
	rm -rf ${reldir}
	
done
