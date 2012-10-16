# CMake generated Testfile for 
# Source directory: /mnt/oqtansTools/oqtans_src/cmake-2.8.9
# Build directory: /mnt/oqtansTools/oqtans_src/cmake-2.8.9
# 
# This file includes the relevent testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
INCLUDE("/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/EnforceConfig.cmake")
ADD_TEST(SystemInformationNew "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "--system-information" "-G" "Unix Makefiles")
SUBDIRS(Utilities/KWIML)
SUBDIRS(Source/kwsys)
SUBDIRS(Utilities/cmzlib)
SUBDIRS(Utilities/cmcurl)
SUBDIRS(Utilities/cmcompress)
SUBDIRS(Utilities/cmbzip2)
SUBDIRS(Utilities/cmlibarchive)
SUBDIRS(Utilities/cmexpat)
SUBDIRS(Source/CursesDialog/form)
SUBDIRS(Source)
SUBDIRS(Utilities)
SUBDIRS(Tests)
SUBDIRS(Docs)
