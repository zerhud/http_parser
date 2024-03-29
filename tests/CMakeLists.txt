cmake_minimum_required(VERSION 3.18)
project(http_parser_tests LANGUAGES CXX C VERSION 0.2.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_SPEED_TESTS "compile and execute speed tests" ON)
option(ENABLE_BROKEN_TESTS "compile and execute speed tests" OFF)
remove_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package(Threads REQUIRED)

macro(add_exe name)
	add_executable(${name} src/${name}.cpp)
	source_group(TREE "${CMAKE_CURRENT_LIST_DIR}/src" FILES src/${name}.cpp)
	target_link_libraries(${name} PRIVATE http_parser ${Boost_LIBRARIES})
	if(ENABLE_SPEED_TESTS)
		target_compile_definitions(${name} PRIVATE ENABLE_SPEED_TESTS)
	endif()
	if(ENABLE_BROKEN_TESTS)
		target_compile_definitions(${name} PRIVATE ENABLE_BROKEN_TESTS)
	endif()
endmacro()
macro(add_unit_test name)
	add_exe(test_${name})
	add_test(NAME ${name} COMMAND test_${name})
endmacro()

add_unit_test(uri_parser)
add_unit_test(generator)
add_unit_test(pos_string_view)
add_unit_test(messages)
add_unit_test(http1_parsers)
add_unit_test(parser)
add_unit_test(utils)
add_unit_test(acceptors)
add_unit_test(router)


# linux specefiec tests
if(UNIX)

#add_unit_test(integrated)

#find_package(Boost COMPONENTS fiber context REQUIRED)

#target_link_libraries(test_integrated PRIVATE ${Boost_LIBRARIES} Threads::Threads)

endif(UNIX)
