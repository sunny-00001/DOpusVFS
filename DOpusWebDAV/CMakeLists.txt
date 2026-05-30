cmake_minimum_required(VERSION 3.15)

project(DOpusWebDAV VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DOpusWebDAV is a Directory Opus VFS plugin and can only be built on Windows.")
endif()

set(SOURCES
    src/DOpusWebDAV.cpp
    src/WebDAVClient.cpp
    src/pugixml.cpp
    src/resource.rc
)

add_library(DOpusWebDAV SHARED ${SOURCES})

target_include_directories(DOpusWebDAV PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DOpusWebDAV PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_compile_features(DOpusWebDAV PRIVATE cxx_std_17)

target_link_libraries(DOpusWebDAV PRIVATE
    Winhttp
    Crypt32
    Credui
    Shell32
    User32
    Advapi32
)

set_target_properties(DOpusWebDAV PROPERTIES
    OUTPUT_NAME "DOpusWebDAV"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

add_custom_command(TARGET DOpusWebDAV POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../DOpusWebDAV.lib" "${CMAKE_SOURCE_DIR}/../DOpusWebDAV.exp"
    COMMENT "Cleaning up .lib and .exp files"
)