# https://cmake.org/cmake/help/book/mastering-cmake/chapter/Finding%20Packages.html
# & https://cmake.org/cmake/help/latest/command/find_package.html#package-file-interface-variables
if (DEFINED SPIRV-Headers_SOURCE_DIR)
  set(SPIRV-Headers_INCLUDE_DIRS ${SPIRV-Headers_SOURCE_DIR}/include)
	set(SPIRV-Headers_FOUND 1)
elseif(SPIRV-Headers_FIND_REQUIRED)
	message(FATAL_ERROR "No SPIRV-Headers found; add it to the project with `add_subdirectory` or set `-DSPIRV-Headers_SOURCE_DIR`")
endif()
