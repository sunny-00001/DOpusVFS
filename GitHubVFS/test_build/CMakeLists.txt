cmake_minimum_required(VERSION 3.10)
project(test_path_parse)

add_executable(test_path_parse test_path_parse.cpp)
set_target_properties(test_path_parse PROPERTIES CXX_STANDARD 14)