cmake_minimum_required(VERSION 3.12)
project(mara)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if (IPO_SUPPORTED)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif ()

option(MARA_STATIC "Whether to build a static library for mara" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_DEBUG_POSTFIX _d)

function (setup_library LIBRARY_NAME IS_STATIC SOURCES)
	if (IS_STATIC)
		add_library(${LIBRARY_NAME} STATIC ${SOURCES})
	else ()
		string(TOUPPER ${LIBRARY_NAME} LIBRARY_NAME_UPPER)
		add_library(${LIBRARY_NAME} SHARED ${SOURCES})
		target_compile_definitions(${LIBRARY_NAME} PUBLIC ${LIBRARY_NAME_UPPER}_SHARED)
		target_compile_definitions(${LIBRARY_NAME} PRIVATE ${LIBRARY_NAME_UPPER}_BUILD)
		set_target_properties(${LIBRARY_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)
	endif ()

	if (NOT MSVC)
		target_link_options(${LIBRARY_NAME} PRIVATE
			-Wl,--exclude-libs,ALL
			-Wl,--no-whole-archive
		)
	endif ()
endfunction ()

if (MSVC)
	# 4200: Flexible array member is a standard feature since C99
	add_compile_options(/W4 /WX /wd4200)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

	# Because CMake can't set proper flags even with CMAKE_INTERPROCEDURAL_OPTIMIZATION_Release
	set(RELEASE_COMPILER_FLAGS "/Gy;/Ob2;/GL;/Oi")
	set(RELEASE_LINKER_FLAGS "/LTCG")

	add_compile_options("$<$<CONFIG:Release>:${RELEASE_COMPILER_FLAGS}>")
	add_link_options("$<$<CONFIG:Release>:${RELEASE_LINKER_FLAGS}>")

	add_compile_options("$<$<CONFIG:RelWithDebInfo>:${RELEASE_COMPILER_FLAGS}>")
	add_link_options("$<$<CONFIG:RelWithDebInfo>:${RELEASE_LINKER_FLAGS}>")
else()
	add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(cli)
