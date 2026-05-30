cmake_minimum_required(VERSION 3.16)
project(RegistryVFS VERSION 5.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(RegistryVFS SHARED
    src/RegistryVFS.cpp
    src/resource.rc
)

target_include_directories(RegistryVFS PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(RegistryVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(RegistryVFS PRIVATE
    advapi32
    Shell32
    Comctl32
    Comdlg32
    Gdi32
    User32
)

set_target_properties(RegistryVFS PROPERTIES
    OUTPUT_NAME "RegistryVFS"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(RegistryVFS PRIVATE /W4 /utf-8)
    target_link_options(RegistryVFS PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/RegistryVFS.def)
endif()

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(RegistryVFS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
    )
endforeach()
