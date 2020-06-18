add_library(mara
	context.c
	strpool.c
	lexer.c
	value_nanbox.c
	value_union.c
	gc.c
	string.c
	thread.c
	stack.c
	ptr_map.c
)
target_include_directories(mara PUBLIC
	../include
	../deps
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
