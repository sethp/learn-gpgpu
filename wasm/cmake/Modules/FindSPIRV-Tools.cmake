
# https://cmake.org/cmake/help/book/mastering-cmake/chapter/Finding%20Packages.html
# & https://cmake.org/cmake/help/latest/command/find_package.html#package-file-interface-variables
if (DEFINED spirv-tools_SOURCE_DIR)
  set(SPIRV-Tools_INCLUDE_DIRS ${spirv-tools_SOURCE_DIR}/include)
  set(SPIRV-Tools_LIBRARIES SPIRV-Tools)
	set(SPIRV-Tools_FOUND 1)
elseif(SPIRV-Tools_FIND_REQUIRED)
	message(FATAL_ERROR "No SPIRV-Tools found; add it to the project with `add_subdirectory` or set `-Dspirv-tools_SOURCE_DIR`")
endif()
