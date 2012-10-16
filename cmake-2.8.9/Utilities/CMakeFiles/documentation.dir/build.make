# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Bootstrap.cmk/cmake

# The command to remove a file.
RM = /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Bootstrap.cmk/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/oqtansTools/oqtans_src/cmake-2.8.9

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/oqtansTools/oqtans_src/cmake-2.8.9

# Utility rule file for documentation.

# Include the progress variables for this target.
include Utilities/CMakeFiles/documentation.dir/progress.make

Utilities/CMakeFiles/documentation: Docs/ctest.txt
Utilities/CMakeFiles/documentation: Docs/cpack.txt
Utilities/CMakeFiles/documentation: Docs/ccmake.txt
Utilities/CMakeFiles/documentation: Docs/cmake.txt

Docs/ctest.txt: bin/ctest
Docs/ctest.txt: Utilities/Doxygen/authors.txt
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/oqtansTools/oqtans_src/cmake-2.8.9/CMakeFiles $(CMAKE_PROGRESS_1)
	@echo "Generating ../Docs/ctest.txt"
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities && ../bin/ctest --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ctest.txt --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ctest.html --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ctest.1 --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ctest.docbook

Docs/cpack.txt: bin/cpack
Docs/cpack.txt: Utilities/Doxygen/authors.txt
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/oqtansTools/oqtans_src/cmake-2.8.9/CMakeFiles $(CMAKE_PROGRESS_2)
	@echo "Generating ../Docs/cpack.txt"
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities && ../bin/cpack --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cpack.txt --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cpack.html --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cpack.1 --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cpack.docbook

Docs/ccmake.txt: bin/ccmake
Docs/ccmake.txt: Utilities/Doxygen/authors.txt
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/oqtansTools/oqtans_src/cmake-2.8.9/CMakeFiles $(CMAKE_PROGRESS_3)
	@echo "Generating ../Docs/ccmake.txt"
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities && ../bin/ccmake --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ccmake.txt --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ccmake.html --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ccmake.1 --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/ccmake.docbook

Docs/cmake.txt: bin/cmake
Docs/cmake.txt: Utilities/Doxygen/authors.txt
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/oqtansTools/oqtans_src/cmake-2.8.9/CMakeFiles $(CMAKE_PROGRESS_4)
	@echo "Generating ../Docs/cmake.txt"
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities && ../bin/cmake --copyright /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/Copyright.txt --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake.txt --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake.html --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake.1 --help-full /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake.docbook --help-policies /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-policies.txt --help-policies /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-policies.html --help-policies /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakepolicies.1 --help-properties /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-properties.txt --help-properties /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-properties.html --help-properties /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakeprops.1 --help-variables /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-variables.txt --help-variables /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-variables.html --help-variables /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakevars.1 --help-modules /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-modules.txt --help-modules /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-modules.html --help-modules /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakemodules.1 --help-commands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-commands.txt --help-commands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-commands.html --help-commands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakecommands.1 --help-compatcommands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-compatcommands.txt --help-compatcommands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmake-compatcommands.html --help-compatcommands /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Docs/cmakecompat.1

documentation: Utilities/CMakeFiles/documentation
documentation: Docs/ctest.txt
documentation: Docs/cpack.txt
documentation: Docs/ccmake.txt
documentation: Docs/cmake.txt
documentation: Utilities/CMakeFiles/documentation.dir/build.make
.PHONY : documentation

# Rule to build all files generated by this target.
Utilities/CMakeFiles/documentation.dir/build: documentation
.PHONY : Utilities/CMakeFiles/documentation.dir/build

Utilities/CMakeFiles/documentation.dir/clean:
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities && $(CMAKE_COMMAND) -P CMakeFiles/documentation.dir/cmake_clean.cmake
.PHONY : Utilities/CMakeFiles/documentation.dir/clean

Utilities/CMakeFiles/documentation.dir/depend:
	cd /mnt/oqtansTools/oqtans_src/cmake-2.8.9 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/oqtansTools/oqtans_src/cmake-2.8.9 /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities /mnt/oqtansTools/oqtans_src/cmake-2.8.9 /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities /mnt/oqtansTools/oqtans_src/cmake-2.8.9/Utilities/CMakeFiles/documentation.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : Utilities/CMakeFiles/documentation.dir/depend

