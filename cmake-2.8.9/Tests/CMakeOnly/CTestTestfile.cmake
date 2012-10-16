# CMake generated Testfile for 
# Source directory: /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly
# Build directory: /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly
# 
# This file includes the relevent testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
ADD_TEST(CMakeOnly.LinkInterfaceLoop "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=LinkInterfaceLoop" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
SET_TESTS_PROPERTIES(CMakeOnly.LinkInterfaceLoop PROPERTIES  TIMEOUT "90")
ADD_TEST(CMakeOnly.CheckSymbolExists "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=CheckSymbolExists" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.CheckCXXSymbolExists "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=CheckCXXSymbolExists" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.CheckCXXCompilerFlag "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=CheckCXXCompilerFlag" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.CheckLanguage "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=CheckLanguage" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.AllFindModules "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=AllFindModules" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.TargetScope "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=TargetScope" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
ADD_TEST(CMakeOnly.ProjectInclude "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/bin/cmake" "-DTEST=ProjectInclude" "-DCMAKE_ARGS=-DCMAKE_PROJECT_ProjectInclude_INCLUDE=/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/ProjectInclude/include.cmake" "-P" "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/Test.cmake")
