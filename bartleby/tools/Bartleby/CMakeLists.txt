include(AddLLVM)

add_llvm_tool(bartleby-tool Bartleby.cpp)

target_include_directories(bartleby-tool SYSTEM PRIVATE "${LLVM_INCLUDE_DIRS}")
target_link_libraries(bartleby-tool PRIVATE Bartleby)
set_target_properties(
  bartleby-tool
  PROPERTIES EXPORT_COMPILE_COMMANDS ON
             CXX_STANDARD "17"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
             OUTPUT_NAME "bartleby")
