# mfc-tools is needed because of MyLock.h in stdafx.h
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Common ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib ${Boost_INCLUDE_DIRS} ${MFC_TOOLS_INCLUDE_DIRS} ${ATL_SERVER_INCLUDE_DIRS})

set(USE_BOOST_ABK ON CACHE BOOL "Prefer Boost version of ABKClient")

if(NOT USE_BOOST)
	set(USE_BOOST_ABK OFF)
endif()

if(MINGW_COMPATIBLE AND NOT USE_BOOST_ABK)
	message(SEND_ERROR "No AbkClient implementation possible")
endif()

## Prefer modern precompiled headers via add_pch() helper
if(NOT MINGW_COMPATIBLE)
	message(STATUS "Adding LibAbkClient")
	file(GLOB LIB_ABK_SRC_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/Common/*.cpp)

	# When we use boost, we provide another AbkClient
	if(NOT USE_BOOST_ABK)
		file(GLOB LIB_ABK_SRC_LIB ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/*.cpp)
	endif()

endif()

if(USE_BOOST_ABK)
  message(STATUS "Soup include dir (abk): ${SOUP_INCLUDE_DIRS}")
  if (NOT SOUP_INCLUDE_DIRS)
    message(SEND_ERROR "Soup include dir not found")
  endif()


	message(STATUS "Adding LibAbkClientBoost")
	file(GLOB LIB_ABK_BOOST_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonFormatter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/CrossPlatform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/AbkServerEvent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/AbkClientDaq.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/AbkClientVar.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/AbkServerFinder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonParserAtl.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/AbkClient.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/AbkClientAbstraction.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/AbkClientSoup.cpp
	)

	# Prefer our stdafx when possible (source files in ClientLib will still use theirs)
	set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost ${INCLUDE_DIRS})
endif()

set(LIB_ABK_SRC ${LIB_ABK_SRC_COMMON} ${LIB_ABK_SRC_LIB} ${LIB_ABK_BOOST_SRC})
add_library(LibAbkClient ${LIB_ABK_SRC})
# Export include directories to the target
target_include_directories(LibAbkClient PUBLIC ${INCLUDE_DIRS})

if(USE_BOOST_ABK)
  target_compile_definitions(LibAbkClient PUBLIC -DBOOST_ABK)
  target_include_directories(LibAbkClient PRIVATE ${SOUP_INCLUDE_DIRS})
  target_link_libraries(LibAbkClient PRIVATE ${SOUP_LIBRARY})
  target_precompile_headers(LibAbkClient PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/stdafx.h>")

	target_link_libraries(LibAbkClient PUBLIC AfxWrapper)
else()
	target_precompile_headers(LibAbkClient PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/stdafx.h>")
endif()



if(BUILD_TESTS)

if(USE_BOOST)
if(Boost_FOUND)
	add_executable(TestAbk EXCLUDE_FROM_ALL Tests/main.cpp Tests/test_client.cpp)
	set(ABK_LIBS ws2_32 ole32 oleaut32)

	# MinGW doesn't need/have this
	if(MSVC)
		set(ABK_LIBS ${ABK_LIBS} comsuppw)
	endif()

	if(NOT MINGW_COMPATIBLE)
		set(ABK_LIBS ${ABK_LIBS} MfcTools)
	endif()

	target_link_libraries(TestAbk LibAbkClient ${Boost_LIBRARIES} ${ABK_LIBS})
else()
	message(WARNING "Boost not found")
endif(Boost_FOUND)
endif(USE_BOOST)

endif()