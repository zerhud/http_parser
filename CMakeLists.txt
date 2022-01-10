cmake_minimum_required(VERSION 3.18)
project(std_http_utils LANGUAGES CXX C VERSION 0.2.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "compile and execute tests" ON)

include(GNUInstallDirs)

file(
	GLOB_RECURSE headers
	LIST_DIRECTORIES true
	RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
	"*.hpp"
	)
file(
	GLOB_RECURSE sources
	LIST_DIRECTORIES true
	RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
	"*.cpp"
	)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${headers} ${sources})

add_library(${PROJECT_NAME} STATIC ${headers} ${sources})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
