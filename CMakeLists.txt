# mfc-tools is needed because of MyLock.h in stdafx.h
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Common ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib)
set(ABK_INCLUDE_DIRS ${INCLUDE_DIRS} PARENT_SCOPE)

file(GLOB LIB_ABK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Common/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ClientLib/*.cpp)
include_directories(${INCLUDE_DIRS} ${MFC_TOOLS_INCLUDE_DIRS} ${ATL_SERVER_INCLUDE_DIRS})
add_library(LibAbkClient ${LIB_ABK_SRC})