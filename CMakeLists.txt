cmake_minimum_required(VERSION 3.13.4)
project(Bartleby)

set(BARTLEBY_LLVM_VERSION "18" CACHE STRING "LLVM version to use")

find_package(LLVM REQUIRED CONFIG)
if (NOT PACKAGE_FIND_VERSION_MAJOR EQUAL 18)
  message(FATAL_ERROR "LLVM 18 is required")
endif()
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_subdirectory(bartleby)
