add_executable(tests
	main.c
	context.c
	strpool.c
)
target_link_libraries(tests PRIVATE munit mara)

add_test(
	NAME tests
	COMMAND tests
)
