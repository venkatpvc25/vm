cmake_minimum_required(VERSION 3.12)
project(my_app C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(vm)

add_executable(main main.c)

target_link_libraries(main PRIVATE vm_lib)