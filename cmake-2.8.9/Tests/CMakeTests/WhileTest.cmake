set(NUMBERS "")
set(COUNT 0)

while(COUNT LESS 200)
    set(NUMBERS "${NUMBERS} ${COUNT}")
    set(COUNT "2${COUNT}")

    set(NCOUNT 3)
    while(NCOUNT LESS 31)
        set(NUMBERS "${NUMBERS} ${NCOUNT}")
        set(NCOUNT "${NCOUNT}0")
    endwhile()
endwhile(COUNT LESS 200)

if(NOT NUMBERS STREQUAL " 0 3 30 20 3 30")
    message(SEND_ERROR "while loop nesting error, result: '${NUMBERS}'")
endif()

set(Missing-Argument-RESULT 1)
set(Missing-Argument-STDERR ".*CMake Error at (/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Missing-Argument.cmake:1 \\(while\\):.*while called with incorrect number of arguments.*")

include("/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/CheckCMakeTest.cmake")
check_cmake_test(While
  Missing-Argument
)

set(Missing-Endwhile-RESULT 1)
set(Missing-Endwhile-STDERR ".*CMake Error in (/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Missing-Endwhile.cmake:.*A logical block opening on the line.*(/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Missing-Endwhile.cmake:1 \\(while\\).*is not closed\\..*")

check_cmake_test(While
  Missing-Endwhile
)

set(Endwhile-Mismatch-RESULT 0)
set(Endwhile-Mismatch-STDERR ".*CMake Warning \\(dev\\) in (/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Endwhile-Mismatch.cmake:.*A logical block opening on the line.*(/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Endwhile-Mismatch.cmake:1 \\(while\\).*with mis-matching arguments\\..*")

check_cmake_test(While
  Endwhile-Mismatch
)

set(Endwhile-Alone-RESULT 1)
set(Endwhile-Alone-STDERR ".*CMake Error at (/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Endwhile-Alone.cmake:1 \\(endwhile\\):.*An ENDWHILE command was found outside of a proper WHILE ENDWHILE.*structure\\.\n.*$")

check_cmake_test(While
  Endwhile-Alone
)

set(Endwhile-Alone-Args-RESULT 1)
set(Endwhile-Alone-Args-STDERR ".*CMake Error at (/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeTests/)?While-Endwhile-Alone-Args.cmake:1 \\(endwhile\\):.*An ENDWHILE command was found outside of a proper WHILE ENDWHILE.*structure\\.  Or its arguments did not.*$")

check_cmake_test(While
  Endwhile-Alone-Args
)
