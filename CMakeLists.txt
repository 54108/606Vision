cmake_minimum_required(VERSION 3.0.0)
project(606Vision VERSION 0.1.0)

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

INCLUDE_DIRECTORIES(${InferenceEngine_INCLUDE_DIRS})
include_directories(/usr/include/ie)

add_executable(606Vision main.cpp)

include_directories(Utils)
add_subdirectory(Utils)
target_link_libraries(606Vision Utils)

include_directories(Camera)
add_subdirectory(Camera)
target_link_libraries(606Vision Camera)

include_directories(Detector)
add_subdirectory(Detector)
target_link_libraries(606Vision Detector)

include_directories(PoseSolver)
add_subdirectory(PoseSolver)
target_link_libraries(606Vision PoseSolver)

include_directories(Serial)
add_subdirectory(Serial)
target_link_libraries(606Vision Serial)

# Set built binary to ~/bin
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")

# Set built library to ~/lib
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")