cmake_minimum_required(VERSION 3.17)

project(HID_Parser_Tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
	arena_allocator.cpp
	hid_gamecube_mapping.cpp
	hid_parser.cpp
	hid_tests.cpp
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})