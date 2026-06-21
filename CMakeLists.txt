cmake_minimum_required(VERSION 3.11)

set(NTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(NTF_INC_DIR "${NTF_DIR}/include")
set(NTF_SRC_DIR "${NTF_DIR}/src")
set(NTF_TEST_DIR "${NTF_DIR}/test")

set(NTF_VER_MAJ 0)
set(NTF_VER_MIN 1)
set(NTF_VER_REV 0)
set(NTF_VER "${NTF_VER_MAJ}.${NTF_VER_MIN}.${NTF_VER_REV}")

project(ntfstl VERSION ${NTF_VER} LANGUAGES CXX)

option(NTF_BUILD_TESTS "Build tests" OFF)
option(NTF_EMBED_GIT "Embed git info in version string" ON)

if(NTF_EMBED_GIT)
  execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${NTF_DIR}"
    OUTPUT_VARIABLE NTF_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${NTF_DIR}"
    OUTPUT_VARIABLE NTF_BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY "${NTF_DIR}"
    OUTPUT_VARIABLE NTF_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(NTF_VERSION "${NTF_VER}-${NTF_BUILD_ID}-${NTF_GIT_TAG} (${NTF_GIT_BRANCH})")
else()
  set(NTF_VERSION "${NTF_VER}")
endif()

message(STATUS "ntfstl: Setting up version ${NTF_VERSION}...")

file(GLOB NTF_SOURCES "${NTF_SRC_DIR}/*.cpp")
file(GLOB_RECURSE NTF_HEADERS "${NTF_INC_DIR}/ntfstl/**.hpp")
configure_file("${NTF_INC_DIR}/ntfstl/version.hpp.in" "include/ntfstl/version.hpp" @ONLY)
list(APPEND NTF_INCLUDES "${NTF_DIR}/include")
list(APPEND NTF_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}/include")

# libfmt
find_package(fmt REQUIRED)
list(APPEND NTF_LINKS fmt::fmt)

add_library(${PROJECT_NAME} STATIC ${NTF_SOURCES} ${NTF_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX CXX_STANDARD 20)

#if (NTF_BUILD_TESTS)
  message(STATUS "ntfstl: Tests enabled")
  add_subdirectory(${NTF_TEST_DIR})
  #endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${NTF_INCLUDES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${NTF_LINKS})
# target_compile_definitions(${PROJECT_NAME} PUBLIC ${NTF_DEFS})
