cmake_minimum_required(VERSION 3.0.0)
project(606Vision VERSION 0.1.0)
# find_package(glfw3 REQUIRED)
# find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 20)

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()

add_compile_definitions(PROJECT_DIR="${PROJECT_SOURCE_DIR}")

add_compile_options(-Wall -Werror=return-type -Wno-unused-variable)
if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_compile_options(-O3 -flto -march=native)
elseif (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    add_compile_options(-O3 -flto -march=native -g)
elseif (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
    add_compile_options(-Os -flto -march=native)
else ()
    add_compile_options(-O0 -g)
endif ()

add_executable(606Vision main.cpp)
target_link_libraries(606Vision GL glfw)

include_directories(Imgui)
add_subdirectory(Imgui)
target_link_libraries(606Vision GL glfw Imgui)

include_directories(Camera)
add_subdirectory(Camera)
target_link_libraries(606Vision Camera)

