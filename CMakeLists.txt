cmake_minimum_required(VERSION 3.10)
project(LibAbk)

option(BUILD_ABK_CLIENT "Build AbkClient library" ON)
option(BUILD_ABK_SERVER "Build AbkServer library" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_ABK_EXAMPLES "Build examples" ON)

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
	pkg_check_modules(SOUP REQUIRED IMPORTED_TARGET libsoup-3.0)
endif()

if(USE_BOOST)
	find_package(Boost 1.78.0 REQUIRED COMPONENTS atomic date_time filesystem unit_test_framework thread chrono regex json)
	message(STATUS "Using Boost")
	add_definitions(-DUSE_BOOST)
endif()

# AfxWrapper is a wrapper around Win32 functions as well as a few MFC classes (like CString).
# As it can have different implementations depending on the required functionality,
# a minimal implementation is provided as part of this project.
# It needs to be interchangeable with variants that can be provided by the user.
if(NOT TARGET AfxWrapper)
	if (NOT WIN32)
		add_subdirectory(AfxWrapper)
	else()
		# AfxWrapper is not needed on Windows, treat it as a dummy library.
		# If really needed, maybe add the libraries required as dependencies
		add_library(AfxWrapper INTERFACE)
	endif()
endif()

if (BUILD_ABK_CLIENT)
	include(${CMAKE_CURRENT_SOURCE_DIR}/AbkClient.cmake)
endif()

if (BUILD_ABK_SERVER)
	add_subdirectory(ServerLib)
	if (BUILD_ABK_EXAMPLES)
		add_subdirectory(ServerExample)
	endif()
endif()