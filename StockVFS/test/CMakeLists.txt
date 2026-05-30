cmake_minimum_required(VERSION 3.16)
project(StockVFS_Test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_stock
    test_stock.cpp
    ${CMAKE_SOURCE_DIR}/../src/StockClient.cpp
)

target_include_directories(test_stock PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../include
)

target_compile_definitions(test_stock PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(test_stock PRIVATE
    advapi32
    Shell32
    Winhttp
)

if(MSVC)
    target_compile_options(test_stock PRIVATE /W4 /utf-8)
endif()

add_executable(test_market_list
    test_market_list.cpp
    ${CMAKE_SOURCE_DIR}/../src/StockClient.cpp
)

target_include_directories(test_market_list PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../include
)

target_compile_definitions(test_market_list PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(test_market_list PRIVATE
    advapi32
    Shell32
    Winhttp
)

if(MSVC)
    target_compile_options(test_market_list PRIVATE /W4 /utf-8)
endif()

add_executable(test_api_raw
    test_api_raw.cpp
)

target_compile_definitions(test_api_raw PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(test_api_raw PRIVATE
    Winhttp
)

if(MSVC)
    target_compile_options(test_api_raw PRIVATE /W4 /utf-8)
endif()
