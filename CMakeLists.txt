get_native_precompiled_header(LibAbkClient)


# mfc-tools is needed because of MyLock.h in stdafx.h
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost ${CMAKE_CURRENT_SOURCE_DIR}/Common ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib ${Boost_INCLUDE_DIRS} ${MFC_TOOLS_INCLUDE_DIRS})
set(ABK_INCLUDE_DIRS ${INCLUDE_DIRS} PARENT_SCOPE)

if(NOT USE_BOOST)
	file(GLOB LIB_ABK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Common/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/*.cpp)
	include_directories(${INCLUDE_DIRS} ${MFC_TOOLS_INCLUDE_DIRS} ${ATL_SERVER_INCLUDE_DIRS})
	add_library(LibAbkClient ${LIB_ABK_SRC})

	add_native_precompiled_header(LibAbkClient "${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/stdafx.h")
else()
	message(WARNING "Building only AbkBoostClientLib. Everything else will be broken.")
	file(GLOB LIB_ABK_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonFormatter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/CrossPlatform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/AbkServerEvent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Common/JsonParserAtl.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/AbkClientBoost.cpp
	)
	add_library(LibAbkClient ${LIB_ABK_SRC})
	# # Directly force use of ASCII version afxwrapper
	#target_compile_definitions(LibAbkClient PUBLIC ASCII_CHAR)
	#remove_definitions(-DUNICODE -D_UNICODE)
	include_directories(LibAbkClient PUBLIC ${INCLUDE_DIRS})
	message(WARNING "LIBABKCLIENT: ${Boost_INCLUDE_DIRS}")

	add_native_precompiled_header(LibAbkClient "${CMAKE_CURRENT_SOURCE_DIR}/ClientBoost/stdafx.h")
endif()


if(BUILD_TESTS)

if(USE_BOOST)
if(Boost_FOUND)
	add_executable(TestAbk EXCLUDE_FROM_ALL Tests/main.cpp Tests/test_client.cpp)
	set(ABK_LIBS ws2_32 ole32 oleaut32)

	# MingW doesn't need/have this
	if(MSVC)
		set(ABK_LIBS ${ABK_LIBS} comsuppw)
	endif()
	target_link_libraries(TestAbk LibAbkClient ${Boost_LIBRARIES} ${ABK_LIBS})
else()
	message(WARNING "Boost not found")
endif(Boost_FOUND)
endif(USE_BOOST)

endif()