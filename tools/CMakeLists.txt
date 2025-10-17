cmake_minimum_required(VERSION 3.10)
project(PointCloudTools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point cloud generator executable
add_executable(point_cloud_generator point_cloud_generator.cpp)

# Point cloud inspector executable
add_executable(inspect_pointcloud inspect_pointcloud.cpp)

# Enable optimizations for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(point_cloud_generator PRIVATE -O3)
    target_compile_options(inspect_pointcloud PRIVATE -O3)
endif()

# Link math library on Unix systems
if(UNIX)
    target_link_libraries(point_cloud_generator m)
    target_link_libraries(inspect_pointcloud m)
endif()

