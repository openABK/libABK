set(SERVER_LIB_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
set(COMMON_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../Common)

set(COMMON_SOURCES
  CrossPlatform.cpp
  JsonFormatter.cpp
  JsonParser.cpp
  LogQueue.cpp
)

list(TRANSFORM COMMON_SOURCES PREPEND ${COMMON_FOLDER}/)

set(SERVER_LIB_SOURCES
  Daq.cpp
  DiscoveryServer.cpp
  EventQueue.cpp
  LoggerIf.cpp
  Session.cpp
  VarRef.cpp
)

list(TRANSFORM SERVER_LIB_SOURCES PREPEND ${SERVER_LIB_FOLDER}/)
add_library(ServerLib ${SERVER_LIB_SOURCES} ${COMMON_SOURCES})
target_include_directories(ServerLib PUBLIC ${SERVER_LIB_FOLDER} ${COMMON_FOLDER})
target_link_libraries(ServerLib PUBLIC AfxWrapper)
target_precompile_headers(ServerLib PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SERVER_LIB_FOLDER}/stdafx.h>")

