add_executable(tests
	main.c
	context.c
)
target_link_libraries(tests PRIVATE munit mara)

add_test(
	NAME context
	CONFIGURATIONS Debug
    COMMAND tests /context
)
