cmake_minimum_required(VERSION 3.10)
project(gaame)

set(CMAKE_C_STANDARD 99)

# Find SDL2 library
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find SDL2_ttf library
find_package(SDL2_ttf REQUIRED)
include_directories(${SDL2_TTF_INCLUDE_DIRS})

# Find Vulkan library
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

# Add your source files
add_executable(gaame main.c)

# Link SDL2, SDL2_ttf, and Vulkan libraries
target_link_libraries(gaame ${SDL2_LIBRARIES} SDL2_ttf ${Vulkan_LIBRARIES})