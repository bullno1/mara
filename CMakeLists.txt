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

add_library(mara
	src/context.c
	src/strpool.c
)
target_include_directories(mara PUBLIC
	include
	deps
)
target_link_libraries(mara PUBLIC bk)
set(STRICT_FLAGS
	-Wall
	-Wextra
	-Werror
	-pedantic
	-Wno-missing-field-initializers
)
set(SANITIZE_FLAGS
	-fsanitize=undefined
	-fsanitize=address
)
set(CLANG_FAMILY $<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>)
target_compile_options(mara PRIVATE $<${CLANG_FAMILY}:${STRICT_FLAGS}>)
target_compile_options(mara PUBLIC $<$<CONFIG:Debug>:${SANITIZE_FLAGS}>)
target_link_options(mara PUBLIC $<$<CONFIG:Debug>:${SANITIZE_FLAGS}>)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
	enable_testing()
    add_subdirectory(tests)
endif()
