add_library(AfxWrapper STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/ThreadLauncher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/afxwrapper/thread.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/afxwrapper/tchar_ascii.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/afxwrapper/time.cpp
)

target_include_directories(AfxWrapper PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_include_directories(AfxWrapper PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../ClientBoost
)

target_link_libraries(AfxWrapper PUBLIC
  Boost::thread
  Boost::chrono
)