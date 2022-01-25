cmake_minimum_required(VERSION 3.18)
set(build_number $ENV{build_number})
if(NOT build_number)
	set(build_number 0)
endif()
project(std_http_utils LANGUAGES CXX C VERSION 0.2.2.${build_number})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "compile and execute tests" ON)
option(USE_LIBCXX_DEBUG "enable glibcxx debug" ON)

if(USE_LIBCXX_DEBUG AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	message("=== use debug version of libcxx")
	add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
endif()

include(GNUInstallDirs)

file(
	GLOB_RECURSE headers
	LIST_DIRECTORIES true
	RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
	"*.hpp"
	)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${headers} ${sources})

add_library(${PROJECT_NAME} INTERFACE ${headers} ${sources})
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
