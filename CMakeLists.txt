cmake_minimum_required(VERSION 3.10)

message(STATUS "Searching for clang")
find_program(CLANG_CXX_COMPILER "clang++")
if(CLANG_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER})
else()
  message(WARNING "Using default compiler, due to missing Clang")
endif()


message(STATUS "Setting variables")
project(Warehouse CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -Wall
    -Wextra
    -pedantic
    -pthread
    -fcolor-diagnostics
    -g
)

include(FetchContent)

message(STATUS "Downloading spdlog")
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.16.0
)
FetchContent_MakeAvailable(spdlog)
target_compile_options(spdlog PRIVATE -w)

message(STATUS "Downloading GoogleTest")
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Uncomment this if you are willing to build this using windows! set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/logs)

message(STATUS "Adding executables")
add_executable(main src/main.cpp)
add_executable(dispatcher src/main_dispatcher.cpp)
add_executable(belt src/main_belt.cpp)
add_executable(express src/main_express.cpp)
add_executable(truck src/main_truck.cpp)

message(STATUS "Setting up tests")
include(GoogleTest)
enable_testing()
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "test/*.cpp")

add_executable(unit_tests ${TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE GTest::gtest_main spdlog::spdlog)
gtest_discover_tests(unit_tests)

message(STATUS "Build configurated")
