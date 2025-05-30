cmake_minimum_required(VERSION 3.11)

set(NTFSTL_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(NTFSTL_INC_DIR "${NTFSTL_DIR}/include")
set(NTFSTL_SRC_DIR "${NTFSTL_DIR}/src")
set(NTFSTL_TEST_DIR "${NTFSTL_DIR}/test")
set(NTFSTL_EXTERN_DIR "${NTFSTL_DIR}/extern")

set(NTFSTL_VER_MAJ 0)
set(NTFSTL_VER_MIN 1)
set(NTFSTL_VER_REV 0)
set(NTFSTL_VER "${NTFSTL_VER_MAJ}.${NTFSTL_VER_MIN}.${NTFSTL_VER_REV}")

project(ntfstl VERSION ${NTFSTL_VER} LANGUAGES CXX)

option(NTFSTL_BUILD_TESTS "Build tests" OFF)
option(NTFSTL_EMBED_GIT "Embed git info in version string" ON)

if(NTFSTL_EMBED_GIT)
  execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${NTFSTL_DIR}"
    OUTPUT_VARIABLE NTFSTL_GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${NTFSTL_DIR}"
    OUTPUT_VARIABLE NTFSTL_BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY "${NTFSTL_DIR}"
    OUTPUT_VARIABLE NTFSTL_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(NTFSTL_VERSION "${NTFSTL_VER}-${NTFSTL_BUILD_ID}-${NTFSTL_GIT_TAG} (${NTFSTL_GIT_BRANCH})")
else()
  set(NTFSTL_VERSION "${NTFSTL_VER}")
endif()

message(STATUS "ntfstl: Setting up version ${NTFSTL_VERSION}...")

file(GLOB NTFSTL_SOURCES "${NTFSTL_SRC_DIR}/*.cpp")
file(GLOB_RECURSE NTFSTL_HEADERS "${NTFSTL_INC_DIR}/ntfstl/**.hpp")
configure_file("${NTFSTL_INC_DIR}/ntfstl/version.hpp.in" "include/ntfstl/version.hpp" @ONLY)
list(APPEND NTFSTL_INCLUDES "${NTFSTL_DIR}/include")
list(APPEND NTFSTL_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}/include")

# libfmt
find_package(fmt REQUIRED)
list(APPEND NTFSTL_LINKS fmt::fmt)

add_library(${PROJECT_NAME} STATIC ${NTFSTL_SOURCES} ${NTFSTL_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX CXX_STANDARD 20)

if (NTFSTL_BUILD_TESTS)
  message(STATUS "ntfstl: Tests enabled")
  add_subdirectory(${NTFSTL_TEST_DIR})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${NTFSTL_INCLUDES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${NTFSTL_LINKS})
# target_compile_definitions(${PROJECT_NAME} PUBLIC ${NTFSTL_DEFS})
