# RMUS Android Rendering Challenge

This challenge tests your ability to efficiently render large-scale 3D point cloud data on Android devices. You'll need to implement a streaming renderer that can handle datasets too large to fit entirely in memory, while maintaining smooth performance.

## Challenge Requirements

### Primary Objectives

1. **Stream Large Point Clouds**: Implement a renderer that can handle point cloud datasets with 50-100+ million points (500MB - 1GB+ of data)
2. **Memory Management**: Since the full dataset cannot fit in RAM, implement intelligent streaming from disk with proper loading/unloading of chunks
3. **Performance Optimization**: Maximize rendering performance (FPS). Higher frame rates indicate better optimization

### Technical Constraints

- **Platform**: Android
- **Graphics API**: OpenGL ES 3.0+
- **Language**: C++ for rendering code (Kotlin/Java for Android integration)
- **Target Devices**: Mid-range to high-end Android phones (assume 2-4GB available RAM), you can use an emulator if needed

## Getting Started

To get started create a fork of this repo and follow the steps outlined below.

Within this repo is a basic starting project based on the Game Activity (C++) example from Android Studio. You do not have to use any of the provided code but it should make your life a little easier.

### 1. Build the Point Cloud Generator

First, you'll need to generate test datasets:

```bash
cd tools
./build.sh  # On Linux/macOS
# or
build.bat   # On Windows
```

### 2. Generate Test Datasets

```bash
cd tools/build

# Small dataset for initial development (1M points, ~16MB)
./point_cloud_generator 1000000 ../app/src/main/assets/pointcloud_1m.pcd

# Medium dataset for testing (10M points, ~160MB)
./point_cloud_generator 10000000 ../app/src/main/assets/pointcloud_10m.pcd

# Large dataset for evaluation (50M points, ~800MB)
./point_cloud_generator 50000000 ../app/src/main/assets/pointcloud_50m.pcd

# Extra large dataset (100M points, ~1.6GB)
./point_cloud_generator 100000000 ../app/src/main/assets/pointcloud_100m.pcd
```

## Point Cloud File Format

The `.pcd` files use a custom binary format optimized for streaming:

### Header
- Magic number: `"PCLOUD1\0"` (8 bytes)
- Version: 1 (uint32)
- Global bounding box: min_x, min_y, min_z, max_x, max_y, max_z (6 floats)
- Total points: uint64
- Chunk count: uint32
- Chunk size: uint32

### Index Table
An array of chunk metadata entries, each containing:
- Chunk bounding box (6 floats)
- Point count (uint32)
- File offset (uint64)

### Point Data
Points are organized into spatial chunks. Each point contains:
- Position: x, y, z (3 floats = 12 bytes)
- Color: r, g, b (3 uint8 = 3 bytes)
- Padding (1 uint8 for alignment)
- **Total: 16 bytes per point**

## Building and Running

We recommend using Android Studio for development and testing of the app.

## Implementation Hints

- You will likely need to create a system for streaming and managing spatially partitioned chunks of the point cloud from disk.
- Which chunks are loaded/unloaded will depend on where the camera is looking.

## Deliverables

Submit your solution by sending the following by email:

1. Complete source code with your implementation, you can simply send a link to your fork
2. A short video showcasing the renderer running on an Android device or emulator

Submissions should be sent to waseef@rmus.com.

## Questions?

If you have questions about the challenge requirements or file format, please reach out to Waseef Nayeem (waseef@rmus.com)

## Resources

- [OpenGL ES 3.0 Reference](https://www.khronos.org/opengles/)
- [Android NDK Documentation](https://developer.android.com/ndk)
- Point cloud tools and format details: See `tools/README.md`

---

**Good luck!** We're excited to see your solutions!