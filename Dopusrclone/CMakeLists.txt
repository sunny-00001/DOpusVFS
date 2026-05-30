cmake_minimum_required(VERSION 3.15)

project(DOpusRclone VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DOpusRclone is a Directory Opus VFS plugin and can only be built on Windows.")
endif()

set(SOURCES
    src/DOpusRclone.cpp
    src/RcloneClient.cpp
    src/resource.rc
)

add_library(DOpusRclone SHARED ${SOURCES})

target_include_directories(DOpusRclone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(DOpusRclone PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_compile_features(DOpusRclone PRIVATE cxx_std_17)

target_link_libraries(DOpusRclone PRIVATE
    Shell32
    User32
    Advapi32
    Winhttp
    Ole32
    Crypt32
)

set_target_properties(DOpusRclone PROPERTIES
    OUTPUT_NAME "DOpusRclone"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

add_custom_command(TARGET DOpusRclone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../DOpusRclone.lib" "${CMAKE_SOURCE_DIR}/../DOpusRclone.exp"
    COMMENT "Cleaning up .lib and .exp files"
)