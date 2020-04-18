cmake_minimum_required(VERSION 3.1)
project(mara VERSION 1.0
             LANGUAGES C)
set(CMAKE_C_STANDARD 99)

add_library(bk INTERFACE)
target_include_directories(bk INTERFACE
	deps/bk/include
)

add_library(munit STATIC
	deps/munit/munit.c
)
target_include_directories(munit PUBLIC deps)

add_subdirectory(src)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING) AND BUILD_TESTING)
	include(CTest)
	enable_testing()
	add_subdirectory(tests)
endif()
