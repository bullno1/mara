set(SOURCES
	"./main.c"
	"./parse.c"
	"./compile.c"
	"./vendor/argparse/argparse.c"
)
add_executable(cli "${SOURCES}")
target_link_libraries(cli mara)
set_target_properties(cli PROPERTIES OUTPUT_NAME "mara")
