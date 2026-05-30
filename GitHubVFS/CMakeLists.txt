cmake_minimum_required(VERSION 3.16)
project(GitHubVFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(GitHubVFS SHARED
    src/GitHubVFS.cpp
    src/GitHubClient.cpp
    src/resource.rc
)

target_include_directories(GitHubVFS PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(GitHubVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(GitHubVFS PRIVATE
    advapi32
    Shell32
    Comctl32
    Winhttp
)

set_target_properties(GitHubVFS PROPERTIES
    OUTPUT_NAME "GitHubVFS"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(GitHubVFS PRIVATE /W4 /utf-8)
    target_link_options(GitHubVFS PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/GitHubVFS.def)
    set_source_files_properties(src/resource.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/resource.h
    )

    add_custom_command(TARGET GitHubVFS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "${CMAKE_SOURCE_DIR}/../GitHubVFS.lib"
            "${CMAKE_SOURCE_DIR}/../GitHubVFS.exp"
        COMMENT "Cleaning up temporary build files"
    )

    add_custom_command(TARGET GitHubVFS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/Dopus/VFSPlugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../GitHubVFS.dll" "D:/Dopus/VFSPlugins/GitHubVFS.dll"
        COMMENT "Deploying GitHubVFS.dll to D:/Dopus/VFSPlugins"
    )
endif()

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(GitHubVFS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
    )
endforeach()
