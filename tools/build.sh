#!/bin/bash

# Build script for point cloud generator

echo "Building Point Cloud Generator..."

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Check if build was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "  Generator: ./build/point_cloud_generator"
    echo "  Inspector: ./build/inspect_pointcloud"
    echo ""
    echo "Usage examples:"
    echo "  ./build/point_cloud_generator                    # Generate 10M points"
    echo "  ./build/point_cloud_generator 100000000          # Generate 100M points"
    echo "  ./build/point_cloud_generator 50000000 test.pcd  # Generate 50M points to test.pcd"
    echo ""
    echo "  ./build/inspect_pointcloud test.pcd              # Inspect point cloud file"
    echo "  ./build/inspect_pointcloud test.pcd --detailed   # Show detailed info"
else
    echo "Build failed!"
    exit 1
fi

