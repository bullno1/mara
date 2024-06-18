set(SOURCES
	"./main.c"
	"./lexer.c"
	"./runtime.c"
)
add_executable(tests "${SOURCES}")

set(MATH_LIB "")
include(CheckLibraryExists)
check_library_exists(m expf "" LIBM)
if(LIBM)
	list(APPEND MATH_LIB "m")
endif()

target_link_libraries(tests mara ${MATH_LIB})
