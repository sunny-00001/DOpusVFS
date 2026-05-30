cmake_minimum_required(VERSION 3.15)

project(ServiceVFS VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "ServiceVFS is a Directory Opus VFS plugin and can only be built on Windows.")
endif()

set(SOURCES
    src/ServiceVFS.cpp
    src/resource.rc
)

add_library(ServiceVFS SHARED ${SOURCES})

target_include_directories(ServiceVFS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ServiceVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_compile_features(ServiceVFS PRIVATE cxx_std_17)

target_link_libraries(ServiceVFS PRIVATE
    Shell32
    User32
    Advapi32
    Gdi32
)

set_target_properties(ServiceVFS PROPERTIES
    OUTPUT_NAME "ServiceVFS"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

add_custom_command(TARGET ServiceVFS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../ServiceVFS.lib" "${CMAKE_SOURCE_DIR}/../ServiceVFS.exp"
    COMMENT "Cleaning up .lib and .exp files"
)