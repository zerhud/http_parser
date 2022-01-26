cmake_minimum_required(VERSION 3.18)
set(build_number $ENV{build_number})
if(NOT build_number)
	set(build_number 0)
endif()
project(std_http_utils LANGUAGES CXX C VERSION 0.2.3.${build_number})

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
target_include_directories(${PROJECT_NAME} INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
	$<INSTALL_INTERFACE:include>
	)

if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	COMPATIBILITY AnyNewerVersion
	)
install(
	TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}-targets
	LIBRARY DESTINATION lib COMPONENT Runtime
	ARCHIVE DESTINATION lib COMPONENT Development
	RUNTIME DESTINATION bin COMPONENT Runtime
	PUBLIC_HEADER DESTINATION include COMPONENT Development
	BUNDLE DESTINATION bin COMPONENT Runtime
	)
configure_package_config_file(
	"${PROJECT_SOURCE_DIR}/cmake/config.cmake.in"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
	)
install(EXPORT ${PROJECT_NAME}-targets DESTINATION lib/cmake/${PROJECT_NAME})
install(
	FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
	DESTINATION lib/cmake/${PROJECT_NAME}
	)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/http_utils/ DESTINATION include/${PROJECT_NAME})

