cmake_minimum_required(VERSION 3.0.0)
project(Imgui)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE src imgui.cpp imgui_widgets.cpp imgui_tables.cpp imgui_impl_opengl3.cpp imgui_impl_glfw.cpp imgui_draw.cpp imgui_demo.cpp)

add_library(Imgui OBJECT ${src})

target_link_libraries(Imgui GL glfw)