cmake_minimum_required(VERSION 3.13.4)
project(Bartleby)

set(BARTLEBY_LLVM_VERSION "16.0" CACHE STRING "LLVM version to use")

find_package(LLVM "${BARTLEBY_LLVM_VERSION}" REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_subdirectory(bartleby)
