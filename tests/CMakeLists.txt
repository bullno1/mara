add_executable(tests
	main.c
	helpers.c
	context.c
	strpool.c
	lexer.c
	value.c
)
target_link_libraries(tests PRIVATE munit mara)

add_test(
	NAME tests
	COMMAND tests
)
