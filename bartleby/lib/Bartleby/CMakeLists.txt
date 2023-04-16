include(AddLLVM)

add_llvm_library(
  Bartleby
  Bartleby.cpp
  Error.cpp
  Symbol.cpp
  OUTPUT_NAME
  "Bartleby"
  LINK_COMPONENTS
  Object
  ObjCopy
  Support)

target_include_directories(
  Bartleby SYSTEM PRIVATE "${LLVM_INCLUDE_DIRS}" "${BARTLEBY_PRIVATE_HEADERS}")
target_include_directories(Bartleby SYSTEM PUBLIC "${BARTLEBY_PUBLIC_HEADERS}")

set_target_properties(
  Bartleby
  PROPERTIES EXPORT_COMPILE_COMMANDS ON
             CXX_STANDARD "17"
             CXX_VISIBILITY_PRESET "hidden"
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
