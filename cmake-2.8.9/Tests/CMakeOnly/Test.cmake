set(source_dir "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/${TEST}")
set(binary_dir "/mnt/oqtansTools/oqtans_src/cmake-2.8.9/Tests/CMakeOnly/${TEST}-build")
file(REMOVE_RECURSE "${binary_dir}")
file(MAKE_DIRECTORY "${binary_dir}")
execute_process(
  COMMAND  ${CMAKE_COMMAND} ${CMAKE_ARGS}
  "${source_dir}" -G "Unix Makefiles"
  WORKING_DIRECTORY "${binary_dir}"
  RESULT_VARIABLE result
  )
if(result)
  message(FATAL_ERROR "CMake failed to configure ${TEST}")
endif()
