find_package(Eigen3 REQUIRED)
find_package(OpenCV 4 REQUIRED)

include_directories(/usr/include/eigen3)
include_directories(/usr/include/opencv4)

file(GLOB_RECURSE src *.cpp)

add_library(Predictor SHARED ${src})

target_link_libraries(Predictor)