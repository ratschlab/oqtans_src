CMAKE_MINIMUM_REQUIRED(VERSION 2.1)

# Settings:
SET(CTEST_DASHBOARD_ROOT                "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTest")
SET(CTEST_SITE                          "ip-10-68-143-93")
SET(CTEST_BUILD_NAME                    "CTestTest-Linux-g++-NoBuild")

SET(CTEST_SOURCE_DIRECTORY              "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTestFailure")
SET(CTEST_BINARY_DIRECTORY              "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTestFailure")
SET(CTEST_CVS_COMMAND                   "/usr/bin/cvs")
SET(CTEST_CMAKE_GENERATOR               "Unix Makefiles")
SET(CTEST_BUILD_CONFIGURATION           "$ENV{CMAKE_CONFIG_TYPE}")
SET(CTEST_COVERAGE_COMMAND              "/usr/bin/gcov")
SET(CTEST_NOTES_FILES                   "${CTEST_SCRIPT_DIRECTORY}/${CTEST_SCRIPT_NAME}")

#CTEST_EMPTY_BINARY_DIRECTORY(${CTEST_BINARY_DIRECTORY})

CTEST_START(Experimental)
#CTEST_UPDATE(SOURCE "${CTEST_SOURCE_DIRECTORY}" RETURN_VALUE res)
CTEST_CONFIGURE(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_BUILD(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
