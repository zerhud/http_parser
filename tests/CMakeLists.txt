cmake_minimum_required(VERSION 3.18)
project(std_http_utils_tests LANGUAGES CXX C VERSION 0.2.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_SPEED_TESTS "compile and execute speed tests" ON)

find_package(Boost COMPONENTS unit_test_framework REQUIRED)

macro(add_exe name)
	add_executable(${name} src/${name}.cpp)
	target_link_libraries(${name} PRIVATE std_http_utils ${Boost_LIBRARIES})
	if(ENABLE_SPEED_TESTS)
		target_compile_definitions(${name} PRIVATE ENABLE_SPEED_TESTS)
	endif()
endmacro()
macro(add_unit_test name)
	add_exe(test_${name})
	add_test(NAME ${name} COMMAND test_${name})
endmacro()

add_unit_test(uri_parser)
add_unit_test(request_generator)
add_unit_test(response_parser)
