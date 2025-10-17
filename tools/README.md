# Point Cloud Tools

Tools for generating and inspecting large synthetic point cloud datasets for testing streaming renderers.

## Tools Included

1. **point_cloud_generator** - Generates synthetic point cloud datasets
2. **inspect_pointcloud** - Inspects and displays information about point cloud files

## Building

### Linux/macOS
```bash
cd tools
mkdir build
cd build
cmake ..
make
```

### Windows (Visual Studio)
```bash
cd tools
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Windows (MinGW)
```bash
cd tools
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

## Usage

### Point Cloud Generator

```bash
./point_cloud_generator [num_points] [output_file]
```

**Arguments:**
- `num_points` - Total number of points to generate (default: 10,000,000)
- `output_file` - Output file path (default: pointcloud.pcd)

**Examples:**
```bash
# Generate 10 million points (default)
./point_cloud_generator

# Generate 100 million points
./point_cloud_generator 100000000

# Generate 50 million points to custom file
./point_cloud_generator 50000000 ../app/src/main/assets/pointcloud_50m.pcd

# Generate small test dataset (1 million points)
./point_cloud_generator 1000000 test_1m.pcd
```

### Point Cloud Inspector

```bash
./inspect_pointcloud <pointcloud_file> [--detailed]
```

**Arguments:**
- `pointcloud_file` - Path to the point cloud file to inspect
- `--detailed` - (Optional) Show detailed chunk information

**Examples:**
```bash
# Inspect a point cloud file
./inspect_pointcloud pointcloud.pcd

# Show detailed chunk information
./inspect_pointcloud pointcloud.pcd --detailed
```

The inspector displays:
- File format information
- Total points and chunk count
- Bounding box dimensions
- Chunk statistics (min/max/avg points per chunk)
- Memory usage estimates
- First 10 chunks (or 20 with --detailed)

## Generated Content

The generator creates a diverse point cloud containing:
- **Terrain** (50%): Multi-octave procedural terrain with height-based coloring
- **Spheres** (variable): Randomly placed colored spheres with Fibonacci distribution
- **Helix** (50%): Colorful spiral/helix structure

## File Format

The output file (.pcd) has the following structure:

### Header (FileHeader)
- Magic number: "PCLOUD1\0"
- Version: 1
- Bounding box (6 floats)
- Total points (uint64)
- Chunk count (uint32)
- Chunk size (uint32)

### Index Table (ChunkMetadata array)
For each chunk:
- Bounding box (6 floats)
- Point count (uint32)
- File offset (uint64)

### Point Data (Point arrays)
For each point:
- Position: x, y, z (3 floats)
- Color: r, g, b (3 uint8)
- Padding (1 uint8)

## Spatial Organization

Points are organized using an octree structure for efficient spatial querying:
- Maximum 100,000 points per leaf node
- Maximum depth of 8 levels
- Minimum 1,000 points per chunk (smaller chunks are discarded)