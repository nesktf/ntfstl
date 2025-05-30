cmake_minimum_required(VERSION 3.11)
include(FetchContent)

project(ntfstl_test CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O0 -g2 -ggdb -Wno-psabi")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O3 -Wno-psabi")

file(GLOB NTFSTLTEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB NTFSTLTEST_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

message(STATUS "ntfstl_test: Fetching Catch2 contents...")
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(Catch2)

message(STATUS "ntfstl_test: Setting up...")
set(CATCH_BUILD_TESTING OFF)
enable_testing()

add_executable(${PROJECT_NAME} ${NTFSTLTEST_SOURCES} ${NTFSTLTEST_HEADERS})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} PRIVATE ntfstl Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
