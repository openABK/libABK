option(ABK_ALLOW_FETCH_CONTENT "Allow fetching content from the server" OFF)
option(ABK_ALLOW_DROGON_FETCH "Allow downloading and building Drogon from GitHub" ON)
option(ABK_USE_SYSTEM_DROGON "Use system-installed Drogon if available" OFF)

if (ABK_USE_SYSTEM_DROGON)
  find_package(Drogon REQUIRED)
  message(STATUS "Using system-installed Drogon")
  set(ABK_DROGON_TARGET Drogon::Drogon)
endif()

if (ABK_ALLOW_FETCH_CONTENT)
  include(FetchContent)
  set(TRANTOR_USE_TLS "openssl")
  if (ABK_ALLOW_DROGON_FETCH)
    message(STATUS "Fetching Drogon from GitHub")
    FetchContent_Declare(
      drogon
      GIT_REPOSITORY https://github.com/drogonframework/drogon.git
      GIT_TAG v1.9.7
    )
    FetchContent_MakeAvailable(drogon)
    set(ABK_DROGON_TARGET drogon)
  endif()
endif()

if (NOT ABK_ALLOW_FETCH_CONTENT AND NOT ABK_USE_SYSTEM_DROGON)
  message(FATAL_ERROR "Drogon is required, but should neither be fetched nor is it available on the system.")
endif()

set(SERVER_EXAMPLE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})

set(SERVER_EXAMPLE_SOURCES
  MyLoggerInterface.cpp
  MyFakeLogger.cpp
  main_drogon.cpp
)

list(TRANSFORM SERVER_EXAMPLE_SOURCES PREPEND ${SERVER_EXAMPLE_FOLDER}/)
add_executable(ServerExample ${SERVER_EXAMPLE_SOURCES})
target_include_directories(ServerExample PUBLIC ${SERVER_EXAMPLE_FOLDER})
target_link_libraries(ServerExample PUBLIC ServerLib ${ABK_DROGON_TARGET})

# Locate glib by using pkg-config, which is the recommended way to find glib
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
  target_link_libraries(ServerExample PUBLIC PkgConfig::GLIB)
endif()