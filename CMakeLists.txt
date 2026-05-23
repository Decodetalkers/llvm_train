cmake_minimum_required(VERSION 4.0)
project(llvm_train)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

find_package(LLVM REQUIRED)

add_executable(llvm_train
  ./main.cpp
)

llvm_map_components_to_libnames(llm_libs support)

target_link_libraries(llvm_train PUBLIC
  ${llm_libs}
)
