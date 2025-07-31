# Recursively find all .c files in vm/src
file(GLOB_RECURSE VM_SOURCE_FILES src/*.c)

# Create static library from source files
add_library(vm_lib STATIC ${VM_SOURCE_FILES})

# Expose vm/include to anything that links to this library
target_include_directories(vm_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
