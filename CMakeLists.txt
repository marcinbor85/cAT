cmake_minimum_required( VERSION 3.0 )

project( libcat LANGUAGES C )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

enable_testing( )

include_directories( ${PROJECT_SOURCE_DIR}/src )

file( GLOB SRC_FILES src/*.c )
add_library( cat SHARED ${SRC_FILES} )
set_target_properties( cat PROPERTIES VERSION 0.3.0 SOVERSION 1 )
target_compile_options( cat PRIVATE -Werror -Wall -Wextra -pedantic )

install( TARGETS cat DESTINATION lib )
install( FILES src/cat.h DESTINATION include/cat )

add_executable( example example/example.c )
target_link_libraries( example cat )

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

add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} --verbose )
add_custom_target( cleanall COMMAND rm -rf Makefile CMakeCache.txt CMakeFiles/ bin/ lib/ cmake_install.cmake CTestTestfile.cmake Testing/ )
add_custom_target( uninstall COMMAND xargs rm < install_manifest.txt )