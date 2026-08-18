add_library(AfxWrapper INTERFACE)

target_include_directories(AfxWrapper INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)
