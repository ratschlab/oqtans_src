CMAKE_MINIMUM_REQUIRED(VERSION 2.1)

# Settings:
SET(CTEST_DASHBOARD_ROOT                "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTest")
SET(CTEST_SITE                          "ip-10-68-143-93")
SET(CTEST_BUILD_NAME                    "CTestTest-Linux-g++-ZeroTimeout")

SET(CTEST_SOURCE_DIRECTORY              "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTestZeroTimeout")
SET(CTEST_BINARY_DIRECTORY              "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CTestTestZeroTimeout")
SET(CTEST_CVS_COMMAND                   "/usr/bin/cvs")
SET(CTEST_CMAKE_GENERATOR               "Unix Makefiles")
SET(CTEST_BUILD_CONFIGURATION           "$ENV{CMAKE_CONFIG_TYPE}")
SET(CTEST_COVERAGE_COMMAND              "/usr/bin/gcov")
SET(CTEST_NOTES_FILES                   "${CTEST_SCRIPT_DIRECTORY}/${CTEST_SCRIPT_NAME}")
SET(CTEST_TEST_TIMEOUT                  2)

CTEST_START(Experimental)
CTEST_CONFIGURE(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_BUILD(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_TEST(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
