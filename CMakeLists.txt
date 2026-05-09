cmake_minimum_required(VERSION 3.18)
project(my_ai_agent)

set(CMAKE_CXX_STANDARD 17)

# Add GGML as a subdirectory
add_subdirectory(external/llama/ggml)

# Header paths
include_directories(src)
include_directories(external/llama/include)
include_directories(external/llama/ggml/include)

# Executable
add_executable(my_agent src/main.cpp)

# Link libraries
target_link_libraries(my_agent PRIVATE 
    "${CMAKE_SOURCE_DIR}/external/llama/lib/llama.lib"
    ggml
    ws2_32 
    advapi32 
    shlwapi
)