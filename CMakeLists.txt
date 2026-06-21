cmake_minimum_required(VERSION 3.25)

set(NTF_VER_MAJ 1)
set(NTF_VER_MIN 0)
set(NTF_VER_REV 0)
set(NTF_VER "${NTF_VER_MAJ}.${NTF_VER_MIN}.${NTF_VER_REV}")

project(ntfstl VERSION ${NTF_VER} LANGUAGES CXX)

option(NTF_TESTS "Build tests" OFF)

file(GLOB_RECURSE NTF_HEADERS
  LIST_DIRECTORIES FALSE
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ntf*.hpp
)

add_library(ntfstl INTERFACE)
add_library(ntfstl::ntfstl ALIAS ntfstl)
target_include_directories(ntfstl
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(ntfstl PROPERTIES
  LINKER_LANGUAGE CXX
  CXX_STANDARD 20
  EXPORT_NAME ntfstl::ntfstl 
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(FETCHCONTENT_QUIET FALSE)
if (NTF_TESTS)
  add_subdirectory(test)
endif()
