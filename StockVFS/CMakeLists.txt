cmake_minimum_required(VERSION 3.16)
project(StockVFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(StockVFS SHARED
    src/StockVFS.cpp
    src/StockClient.cpp
    src/resource.rc
)

target_include_directories(StockVFS PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(StockVFS PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    DOPUS_PLUGIN_HELPER
    VFSPLUGINVERSION=2
)

target_link_libraries(StockVFS PRIVATE
    advapi32
    Shell32
    Comctl32
    Winhttp
    ws2_32
)

set_target_properties(StockVFS PROPERTIES
    OUTPUT_NAME "StockVFS"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(StockVFS PRIVATE /W4 /utf-8)
    target_link_options(StockVFS PRIVATE /DEF:${CMAKE_SOURCE_DIR}/src/StockVFS.def)
    set_source_files_properties(src/resource.rc PROPERTIES
        OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/resource.h
    )

    add_custom_command(TARGET StockVFS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "${CMAKE_SOURCE_DIR}/../StockVFS.lib"
            "${CMAKE_SOURCE_DIR}/../StockVFS.exp"
        COMMENT "Cleaning up temporary build files"
    )

    add_custom_command(TARGET StockVFS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "D:/Dopus/VFSPlugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../StockVFS.dll" "D:/Dopus/VFSPlugins/StockVFS.dll"
        COMMENT "Deploying StockVFS.dll to D:/Dopus/VFSPlugins"
    )
endif()

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(StockVFS PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_SOURCE_DIR}/.."
    )
endforeach()

add_executable(test_market_list test/test_market_list.cpp src/StockClient.cpp)
target_include_directories(test_market_list PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_market_list PRIVATE UNICODE _UNICODE)
target_compile_options(test_market_list PRIVATE /utf-8)
target_link_libraries(test_market_list PRIVATE Winhttp advapi32 Shell32 ws2_32)
set_target_properties(test_market_list PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_search_bj test/test_search_bj.cpp src/StockClient.cpp)
target_include_directories(test_search_bj PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_search_bj PRIVATE UNICODE _UNICODE)
target_compile_options(test_search_bj PRIVATE /utf-8)
target_link_libraries(test_search_bj PRIVATE Winhttp advapi32 Shell32 ws2_32)
set_target_properties(test_search_bj PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_search_columns test/test_search_columns.cpp src/StockClient.cpp)
target_include_directories(test_search_columns PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_search_columns PRIVATE UNICODE _UNICODE)
target_compile_options(test_search_columns PRIVATE /utf-8)
target_link_libraries(test_search_columns PRIVATE Winhttp advapi32 Shell32 ws2_32)
set_target_properties(test_search_columns PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_search_debug test/test_search_debug.cpp)
target_compile_definitions(test_search_debug PRIVATE UNICODE _UNICODE)
target_compile_options(test_search_debug PRIVATE /utf-8)
target_link_libraries(test_search_debug PRIVATE Winhttp ws2_32)
set_target_properties(test_search_debug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_http_debug test/test_http_debug.cpp)
target_compile_options(test_http_debug PRIVATE /utf-8)
target_link_libraries(test_http_debug PRIVATE Winhttp Wininet ws2_32)
set_target_properties(test_http_debug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_encoding test/test_encoding.cpp)
target_compile_options(test_encoding PRIVATE /utf-8)
target_link_libraries(test_encoding PRIVATE Winhttp)
set_target_properties(test_encoding PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_json_parse test/test_json_parse.cpp)
target_compile_options(test_json_parse PRIVATE /utf-8)
target_link_libraries(test_json_parse PRIVATE Winhttp)
set_target_properties(test_json_parse PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_display test/test_display.cpp)
target_compile_options(test_display PRIVATE /utf-8)
target_link_libraries(test_display PRIVATE Winhttp)
set_target_properties(test_display PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_display2 test/test_display2.cpp)
target_compile_options(test_display2 PRIVATE /utf-8)
target_link_libraries(test_display2 PRIVATE Winhttp)
set_target_properties(test_display2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_disk_cache test/test_disk_cache.cpp)
target_compile_options(test_disk_cache PRIVATE /utf-8)
target_link_libraries(test_disk_cache PRIVATE Winhttp Shell32)
set_target_properties(test_disk_cache PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_e2e_cache test/test_e2e_cache.cpp)
target_compile_options(test_e2e_cache PRIVATE /utf-8)
target_link_libraries(test_e2e_cache PRIVATE Winhttp Shell32 ws2_32)
set_target_properties(test_e2e_cache PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_multi_market test/test_multi_market.cpp)
target_compile_options(test_multi_market PRIVATE /utf-8)
target_link_libraries(test_multi_market PRIVATE Winhttp ws2_32)
set_target_properties(test_multi_market PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_sina_quote test/test_sina_quote.cpp)
target_compile_options(test_sina_quote PRIVATE /utf-8)
target_link_libraries(test_sina_quote PRIVATE Winhttp ws2_32)
set_target_properties(test_sina_quote PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_hk_us_api test/test_hk_us_api.cpp)
target_compile_options(test_hk_us_api PRIVATE /utf-8)
target_link_libraries(test_hk_us_api PRIVATE Winhttp ws2_32)
set_target_properties(test_hk_us_api PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_bj_market test/test_bj_market.cpp)
target_compile_options(test_bj_market PRIVATE /utf-8)
target_link_libraries(test_bj_market PRIVATE Winhttp ws2_32)
set_target_properties(test_bj_market PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_phase2 test/test_phase2.cpp)
target_compile_options(test_phase2 PRIVATE /utf-8)
target_link_libraries(test_phase2 PRIVATE Winhttp ws2_32)
set_target_properties(test_phase2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_api_verify test/test_api_verify.cpp)
target_compile_options(test_api_verify PRIVATE /utf-8)
target_link_libraries(test_api_verify PRIVATE Winhttp ws2_32)
set_target_properties(test_api_verify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_sector test/test_em_sector.cpp)
target_compile_options(test_em_sector PRIVATE /utf-8)
target_link_libraries(test_em_sector PRIVATE Winhttp ws2_32)
set_target_properties(test_em_sector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em2 test/test_em2.cpp)
target_compile_options(test_em2 PRIVATE /utf-8)
target_link_libraries(test_em2 PRIVATE Winhttp ws2_32)
set_target_properties(test_em2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em3 test/test_em3.cpp)
target_compile_options(test_em3 PRIVATE /utf-8)
target_link_libraries(test_em3 PRIVATE Winhttp ws2_32)
set_target_properties(test_em3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em4 test/test_em4.cpp)
target_compile_options(test_em4 PRIVATE /utf-8)
target_link_libraries(test_em4 PRIVATE Winhttp ws2_32)
set_target_properties(test_em4 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_index_sector test/test_index_sector.cpp)
target_compile_options(test_index_sector PRIVATE /utf-8)
target_link_libraries(test_index_sector PRIVATE Winhttp ws2_32)
set_target_properties(test_index_sector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_sector_api2 test/test_sector_api2.cpp)
target_compile_options(test_sector_api2 PRIVATE /utf-8)
target_link_libraries(test_sector_api2 PRIVATE Winhttp ws2_32)
set_target_properties(test_sector_api2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_sector_api3 test/test_sector_api3.cpp)
target_compile_options(test_sector_api3 PRIVATE /utf-8)
target_link_libraries(test_sector_api3 PRIVATE Winhttp ws2_32)
set_target_properties(test_sector_api3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_index_sector_vfs test/test_index_sector_vfs.cpp src/StockClient.cpp)
target_include_directories(test_index_sector_vfs PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_index_sector_vfs PRIVATE /utf-8)
target_compile_definitions(test_index_sector_vfs PRIVATE UNICODE _UNICODE)
target_link_libraries(test_index_sector_vfs PRIVATE Winhttp ws2_32 Shell32)
set_target_properties(test_index_sector_vfs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_sina_index_raw test/test_sina_index_raw.cpp)
target_compile_options(test_sina_index_raw PRIVATE /utf-8)
target_compile_definitions(test_sina_index_raw PRIVATE UNICODE _UNICODE)
target_link_libraries(test_sina_index_raw PRIVATE Winhttp ws2_32)
set_target_properties(test_sina_index_raw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_sector_raw test/test_sector_raw.cpp)
target_compile_options(test_sector_raw PRIVATE /utf-8)
target_compile_definitions(test_sector_raw PRIVATE UNICODE _UNICODE)
target_link_libraries(test_sector_raw PRIVATE Winhttp ws2_32)
set_target_properties(test_sector_raw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_sector_raw test/test_em_sector_raw.cpp)
target_compile_options(test_em_sector_raw PRIVATE /utf-8)
target_compile_definitions(test_em_sector_raw PRIVATE UNICODE _UNICODE)
target_link_libraries(test_em_sector_raw PRIVATE Winhttp ws2_32)
set_target_properties(test_em_sector_raw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_sector_client test/test_em_sector_client.cpp src/StockClient.cpp)
target_include_directories(test_em_sector_client PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_em_sector_client PRIVATE /utf-8)
target_compile_definitions(test_em_sector_client PRIVATE UNICODE _UNICODE)
target_link_libraries(test_em_sector_client PRIVATE Winhttp ws2_32 Shell32)
set_target_properties(test_em_sector_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_api test/test_em_api.cpp src/StockClient.cpp)
target_include_directories(test_em_api PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_em_api PRIVATE /utf-8)
target_compile_definitions(test_em_api PRIVATE UNICODE _UNICODE)
target_link_libraries(test_em_api PRIVATE Winhttp ws2_32 Shell32)
set_target_properties(test_em_api PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_debug test/test_em_debug.cpp)
target_compile_options(test_em_debug PRIVATE /utf-8)
target_compile_definitions(test_em_debug PRIVATE UNICODE _UNICODE)
target_link_libraries(test_em_debug PRIVATE Winhttp ws2_32)
set_target_properties(test_em_debug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)

add_executable(test_em_debug2 test/test_em_debug2.cpp)
target_compile_options(test_em_debug2 PRIVATE /utf-8)
target_compile_definitions(test_em_debug2 PRIVATE UNICODE _UNICODE)
target_link_libraries(test_em_debug2 PRIVATE Winhttp ws2_32)
set_target_properties(test_em_debug2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build_test"
)
