cmake_minimum_required(VERSION 3.16)
project(TaskSchedulerVFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(TaskSchedulerVFS SHARED
    src/TaskSchedulerVFS.cpp
    src/resource.rc
)

target_include_directories(TaskSchedulerVFS PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(TaskSchedulerVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(TaskSchedulerVFS PRIVATE
    advapi32
    Shell32
    Comctl32
    Ole32
    OleAut32
    Gdi32
    Comdlg32
    Taskschd
)

set_target_properties(TaskSchedulerVFS PROPERTIES
    OUTPUT_NAME "TaskSchedulerVFS"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(TaskSchedulerVFS PRIVATE /W4 /utf-8)
    target_link_options(TaskSchedulerVFS PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/TaskSchedulerVFS.def)
endif()

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(TaskSchedulerVFS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}"
    )
endforeach()

add_custom_command(TARGET TaskSchedulerVFS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../TaskSchedulerVFS.exp"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/../TaskSchedulerVFS.lib"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/TaskSchedulerVFS.exp"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_SOURCE_DIR}/TaskSchedulerVFS.lib"
)
