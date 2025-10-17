#!/bin/bash

# Generate multiple test datasets of different sizes

GENERATOR="./build/point_cloud_generator"

if [ ! -f "$GENERATOR" ]; then
    echo "Generator not found. Building first..."
    ./build.sh
fi

echo "Generating test datasets..."
echo ""

# Create output directory
mkdir -p test_data

# Small dataset for quick testing (1M points ~ 12 MB)
echo "=== Generating small dataset (1M points) ==="
$GENERATOR 1000000 test_data/pointcloud_1m.pcd
echo ""

# Medium dataset for development (10M points ~ 120 MB)  
echo "=== Generating medium dataset (10M points) ==="
$GENERATOR 10000000 test_data/pointcloud_10m.pcd
echo ""

# Large dataset for testing streaming (50M points ~ 600 MB)
echo "=== Generating large dataset (50M points) ==="
$GENERATOR 50000000 test_data/pointcloud_50m.pcd
echo ""

# Extra large dataset for challenge (100M points ~ 1.2 GB)
echo "=== Generating extra large dataset (100M points) ==="
$GENERATOR 100000000 test_data/pointcloud_100m.pcd
echo ""

echo "=== All datasets generated ==="
ls -lh test_data/*.pcd

