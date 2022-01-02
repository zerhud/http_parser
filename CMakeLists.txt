cmake_minimum_required(VERSION 3.18)
project(stdhttp_utils LANGUAGES CXX C VERSION 0.2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "compile and execute tests" ON)

include(GNUInstallDirs)

file(
	GLOB_RECURSE headers
	LIST_DIRECTORIES true
	RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
	"*.hpp"
	)
source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${headers})

add_library(${PROJECT_NAME} INTERFACE ${headers})
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
