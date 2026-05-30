cmake_minimum_required(VERSION 3.15)

project(RcloneVFS VERSION 2.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "RcloneVFS is a Directory Opus VFS plugin and can only be built on Windows.")
endif()

set(SOURCES
    src/RcloneVFS.cpp
    src/RcloneClient.cpp
    src/PathParser.cpp
    src/RcloneCache.cpp
    src/RcloneFeatures.cpp
    src/ColumnManager.cpp
    src/MenuManager.cpp
    src/DaemonManager.cpp
    src/ConfigManager.cpp
    src/ShareDialog.cpp
    src/VersionsDialog.cpp
    src/SyncDialog.cpp
    src/CheckDialog.cpp
    src/JobDialog.cpp
    src/TrashDialog.cpp
    src/Logger.cpp
    src/ThreadPool.cpp
    src/DebounceRefresh.cpp
    src/BatchOperationManager.cpp
    src/resource.rc
)

add_library(RcloneVFS SHARED ${SOURCES})

target_include_directories(RcloneVFS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(RcloneVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_compile_features(RcloneVFS PRIVATE cxx_std_17)

target_link_libraries(RcloneVFS PRIVATE
    Shell32
    User32
    Advapi32
    Winhttp
    Crypt32
    Shlwapi
    Gdi32
)

set_target_properties(RcloneVFS PROPERTIES
    OUTPUT_NAME "RcloneVFS"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

add_custom_command(TARGET RcloneVFS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../RcloneVFS.lib" "${CMAKE_SOURCE_DIR}/../RcloneVFS.exp"
    COMMENT "Cleaning up .lib and .exp files"
)