cmake_minimum_required(VERSION 3.10)
project(LibAbk)

option(BUILD_ABK_CLIENT "Build AbkClient library" ON)
option(BUILD_TESTS "Build tests" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(/DLINUX)
  add_compile_options(-fstack-protector-strong -Werror=return-type)
  set(LINUX ON)
	set(MINGW_COMPATIBLE ON)
	# NO_ATL is specified for toolchains without ATL support (e.g. MinGW or Linux)
	# NO_WINDOWS is specified for toolchains without Win32 support (e.g. Linux)
	add_definitions(-DNO_ATL -DNO_WINDOWS)
	set(USE_BOOST ON)

	# Find libsoup
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SOUP REQUIRED libsoup-3.0)
endif()

add_subdirectory(AfxWrapper)

if (BUILD_ABK_CLIENT)
	include(${CMAKE_CURRENT_SOURCE_DIR}/AbkClient.cmake)
endif()