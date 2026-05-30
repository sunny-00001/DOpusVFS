cmake_minimum_required(VERSION 3.16)
project(FirewallVFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(FirewallVFS SHARED
    src/FirewallVFS.cpp
    src/resource.rc
)

target_include_directories(FirewallVFS PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(FirewallVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(FirewallVFS PRIVATE
    advapi32
    Shell32
    Comctl32
    Ole32
    OleAut32
)

set_target_properties(FirewallVFS PROPERTIES
    OUTPUT_NAME "FirewallVFS"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(FirewallVFS PRIVATE /W4 /utf-8)
    target_link_options(FirewallVFS PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/FirewallVFS.def)
endif()

add_custom_command(TARGET FirewallVFS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../FirewallVFS.lib" "${CMAKE_SOURCE_DIR}/../FirewallVFS.exp"
    COMMENT "Cleaning up .lib and .exp files"
)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(FirewallVFS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
    )
endforeach()
