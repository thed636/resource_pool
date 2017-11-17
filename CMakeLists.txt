cmake_minimum_required(VERSION 2.6)

project(resource_pool)

option(RESOURCE_POOL_BUILD_TESTS "Enable tests build" OFF)
option(RESOURCE_POOL_BUILD_BENCHMARKS "Enable benchmarks build" OFF)
option(RESOURCE_POOL_BUILD_EXAMPLES "Enable examples build" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_COROUTINES_NO_DEPRECATION_WARNING")

include(./share/find_resource_pool_dependencies.cmake)
find_resource_pool_dependencies()

add_subdirectory(share)
add_subdirectory(include)

if(RESOURCE_POOL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(RESOURCE_POOL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(RESOURCE_POOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
