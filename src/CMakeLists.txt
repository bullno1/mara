set(SOURCES
	"env.c"
	"alloc.c"
	"zone.c"
	"value.c"
	"error.c"
	"vendor.c"
	"lexer.c"
	"parser.c"
	"copy.c"
	"list.c"
	"map.c"
	"symtab.c"
	"debug_info.c"
	"strpool.c"
	"print.c"
	"compiler.c"
	"vm.c"
	"vm_dispatch.c"
	"module.c"
	"core_module.c"
)

setup_library(mara ${MARA_STATIC} "${SOURCES}")
target_include_directories(mara PUBLIC "../include")

# For testing
add_library(mara_internal INTERFACE)
target_include_directories(mara_internal INTERFACE "../")
