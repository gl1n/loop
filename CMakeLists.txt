project(loop)
cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
  ${PROJECT_SOURCE_DIR}/include
)

add_subdirectory(
  ${PROJECT_SOURCE_DIR}/tests
)