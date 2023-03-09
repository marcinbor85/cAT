cmake_minimum_required( VERSION 3.0 )

project( libcat LANGUAGES C )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

add_compile_options("$<$<CONFIG:DEBUG>:-g>")

enable_testing( )

include_directories( ${PROJECT_SOURCE_DIR}/src )

file( GLOB SRC_FILES src/*.c )
add_library( cat SHARED ${SRC_FILES} )
target_include_directories(cat INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)
set_target_properties( cat PROPERTIES VERSION 0.10.1 SOVERSION 1 )
target_compile_options( cat PRIVATE -Werror -Wall -Wextra -pedantic )

install( TARGETS cat DESTINATION lib )
install( FILES src/cat.h DESTINATION include/cat )

add_executable( demo example/demo.c )
target_link_libraries( demo cat )

add_executable( basic example/basic.c )
target_link_libraries( basic cat )

add_executable( unsolicited example/unsolicited.c )
# target_compile_options(unsolicited PRIVATE -g)
target_link_libraries( unsolicited cat )

add_executable( test_parse tests/test_parse.c )
target_link_libraries( test_parse cat )
add_test( test_parse ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_parse )

add_executable( test_run tests/test_run.c )
target_link_libraries( test_run cat )
add_test( test_run ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_run )

add_executable( test_read tests/test_read.c )
target_link_libraries( test_read cat )
add_test( test_read ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_read )

add_executable( test_write tests/test_write.c )
target_link_libraries( test_write cat )
add_test( test_write ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write )

add_executable( test_write_parse tests/test_write_parse.c )
target_link_libraries( test_write_parse cat )
add_test( test_write_parse ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_parse )

add_executable( test_write_int_range tests/test_write_int_range.c )
target_link_libraries( test_write_int_range cat )
add_test( test_write_int_range ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_int_range )

add_executable( test_write_uint_range tests/test_write_uint_range.c )
target_link_libraries( test_write_uint_range cat )
add_test( test_write_uint_range ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_uint_range )

add_executable( test_write_hex_range tests/test_write_hex_range.c )
target_link_libraries( test_write_hex_range cat )
add_test( test_write_hex_range ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_hex_range )

add_executable( test_write_hex_buffer tests/test_write_hex_buffer.c )
target_link_libraries( test_write_hex_buffer cat )
add_test( test_write_hex_buffer ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_hex_buffer )

add_executable( test_write_string_buffer tests/test_write_string_buffer.c )
target_link_libraries( test_write_string_buffer cat )
add_test( test_write_string_buffer ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_write_string_buffer )

add_executable( test_read_args tests/test_read_args.c )
target_link_libraries( test_read_args cat )
add_test( test_read_args ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_read_args )

add_executable( test_test tests/test_test.c )
target_link_libraries( test_test cat )
add_test( test_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_test )

add_executable( test_test_args tests/test_test_args.c )
target_link_libraries( test_test_args cat )
add_test( test_test_args ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_test_args )

add_executable( test_mutex tests/test_mutex.c )
target_link_libraries( test_mutex cat )
add_test( test_mutex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mutex )

add_executable( test_unsolicited_read tests/test_unsolicited_read.c )
target_link_libraries( test_unsolicited_read cat )
add_test( test_unsolicited_read ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_unsolicited_read )

add_executable( test_unsolicited_read_stress tests/test_unsolicited_read_stress.c )
target_link_libraries( test_unsolicited_read_stress cat )
add_test( test_unsolicited_read_stress ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_unsolicited_read_stress )

add_executable( test_unsolicited_read_buffer tests/test_unsolicited_read_buffer.c ${SRC_FILES})
set_target_properties( test_unsolicited_read_buffer PROPERTIES COMPILE_DEFINITIONS "CAT_UNSOLICITED_CMD_BUFFER_SIZE=2" )
add_test( test_unsolicited_read_buffer ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_unsolicited_read_buffer )

add_executable( test_hold_state tests/test_hold_state.c )
target_link_libraries( test_hold_state cat )
add_test( test_hold_state ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_hold_state )

add_executable( test_return_read tests/test_return_read.c )
target_link_libraries( test_return_read cat )
add_test( test_return_read ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_return_read )

add_executable( test_return_write tests/test_return_write.c )
target_link_libraries( test_return_write cat )
add_test( test_return_write ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_return_write )

add_executable( test_unsolicited_test tests/test_unsolicited_test.c )
target_link_libraries( test_unsolicited_test cat )
add_test( test_unsolicited_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_unsolicited_test )

add_executable( test_return_test tests/test_return_test.c )
target_link_libraries( test_return_test cat )
add_test( test_return_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_return_test )

add_executable( test_return_run tests/test_return_run.c )
target_link_libraries( test_return_run cat )
add_test( test_return_run ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_return_run )

add_executable( test_test_only tests/test_test_only.c )
target_link_libraries( test_test_only cat )
add_test( test_test_only ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_test_only )

add_executable( test_search_cmd tests/test_search_cmd.c )
target_link_libraries( test_search_cmd cat )
add_test( test_search_cmd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_search_cmd )

add_executable( test_order tests/test_order.c )
target_link_libraries( test_order cat )
add_test( test_order ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_order )

add_executable( test_cmd_list tests/test_cmd_list.c )
target_link_libraries( test_cmd_list cat )
add_test( test_cmd_list ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_cmd_list )

add_executable( test_var_access tests/test_var_access.c )
target_link_libraries( test_var_access cat )
add_test( test_var_access ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_var_access )

add_executable( test_shortcuts tests/test_shortcuts.c )
target_link_libraries( test_shortcuts cat )
add_test( test_shortcuts ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_shortcuts )

add_executable( test_implicit_write tests/test_implicit_write.c )
target_link_libraries( test_implicit_write cat )
add_test( test_implicit_write ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_implicit_write )

add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} --verbose )
add_custom_target( cleanall COMMAND rm -rf Makefile CMakeCache.txt CMakeFiles/ bin/ lib/ cmake_install.cmake CTestTestfile.cmake Testing/ )
add_custom_target( uninstall COMMAND xargs rm < install_manifest.txt )