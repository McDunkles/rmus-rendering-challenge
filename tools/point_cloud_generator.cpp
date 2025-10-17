#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstring>
#include <memory>
#include <cmath>

#define M_PI 3.14159265358979323846

// Shared data structures
#include "../app/src/main/cpp/PointCloudData.h"

// Octree node for spatial organization
struct OctreeNode {
    BoundingBox bbox;
    std::vector<Point> points;
    std::unique_ptr<OctreeNode> children[8];
    bool is_leaf;
    
    OctreeNode(const BoundingBox& box) : bbox(box), is_leaf(true) {
        for (int i = 0; i < 8; ++i) {
            children[i] = nullptr;
        }
    }
    
    void insert(const Point& point, int max_points_per_leaf, int max_depth, int current_depth = 0) {
        if (!bbox.contains(point.x, point.y, point.z)) {
            return;
        }
        
        if (is_leaf) {
            points.push_back(point);
            
            if (points.size() > max_points_per_leaf && current_depth < max_depth) {
                subdivide(max_points_per_leaf, max_depth, current_depth);
            }
        } else {
            int octant = getOctant(point.x, point.y, point.z);
            children[octant]->insert(point, max_points_per_leaf, max_depth, current_depth + 1);
        }
    }
    
    int getOctant(float x, float y, float z) const {
        float mid_x = (bbox.min_x + bbox.max_x) / 2.0f;
        float mid_y = (bbox.min_y + bbox.max_y) / 2.0f;
        float mid_z = (bbox.min_z + bbox.max_z) / 2.0f;
        
        int octant = 0;
        if (x >= mid_x) octant |= 1;
        if (y >= mid_y) octant |= 2;
        if (z >= mid_z) octant |= 4;
        
        return octant;
    }
    
    void subdivide(int max_points_per_leaf, int max_depth, int current_depth) {
        float mid_x = (bbox.min_x + bbox.max_x) / 2.0f;
        float mid_y = (bbox.min_y + bbox.max_y) / 2.0f;
        float mid_z = (bbox.min_z + bbox.max_z) / 2.0f;
        
        BoundingBox child_boxes[8] = {
            {bbox.min_x, bbox.min_y, bbox.min_z, mid_x, mid_y, mid_z}, // 0
            {mid_x, bbox.min_y, bbox.min_z, bbox.max_x, mid_y, mid_z}, // 1
            {bbox.min_x, mid_y, bbox.min_z, mid_x, bbox.max_y, mid_z}, // 2
            {mid_x, mid_y, bbox.min_z, bbox.max_x, bbox.max_y, mid_z}, // 3
            {bbox.min_x, bbox.min_y, mid_z, mid_x, mid_y, bbox.max_z}, // 4
            {mid_x, bbox.min_y, mid_z, bbox.max_x, mid_y, bbox.max_z}, // 5
            {bbox.min_x, mid_y, mid_z, mid_x, bbox.max_y, bbox.max_z}, // 6
            {mid_x, mid_y, mid_z, bbox.max_x, bbox.max_y, bbox.max_z}  // 7
        };
        
        for (int i = 0; i < 8; ++i) {
            children[i] = std::make_unique<OctreeNode>(child_boxes[i]);
        }
        
        for (const auto& point : points) {
            int octant = getOctant(point.x, point.y, point.z);
            children[octant]->insert(point, max_points_per_leaf, max_depth, current_depth + 1);
        }
        
        points.clear();
        points.shrink_to_fit();
        is_leaf = false;
    }
    
    void collectChunks(std::vector<std::vector<Point>>& chunks, int min_points = 100) {
        if (is_leaf) {
            if (!points.empty() && points.size() >= min_points) {
                chunks.push_back(points);
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                if (children[i]) {
                    children[i]->collectChunks(chunks, min_points);
                }
            }
        }
    }
};

// Generate a terrain-like point cloud
void generateTerrain(std::vector<Point>& points, int count, std::mt19937& rng) {
    for (int i = 0; i < count; ++i) {
        // Create a grid of points centered on the origin
        int grid_size = static_cast<int>(std::sqrt(count));
        if (grid_size == 0) {
            grid_size = 1;
        }
        int gx = i % grid_size;
        int gz = i / grid_size;
        float x = (gx - grid_size / 2.0f) * (100.0f / grid_size);
        float z = (gz - grid_size / 2.0f) * (100.0f / grid_size);
        
        // Multi-octave terrain
        float y = 0.0f;
        y += 10.0f * std::sin(z * 0.1f) * std::cos(x * 0.1f);
        y += 5.0f * std::sin(z * 0.3f) * std::cos(x * 0.3f);
        y += 2.5f * std::sin(z * 0.7f) * std::cos(x * 0.7f);
        
        // Color based on height
        uint8_t r = static_cast<uint8_t>(std::max(0.0f, std::min(128 + y * 5, 255.0f)));
        uint8_t g = static_cast<uint8_t>(std::max(0.0f, std::min(180 + y * 3, 255.0f)));
        uint8_t b = static_cast<uint8_t>(std::max(0.0f, std::min(100 + y * 2, 255.0f)));

        
        points.push_back({x, y, z, r, g, b, 0});
    }
}

// Generate spherical objects
void generateSpheres(std::vector<Point>& points, int num_spheres, std::mt19937& rng) {
    std::uniform_real_distribution<float> pos_dist(-40.0f, 40.0f);
    std::uniform_real_distribution<float> radius_dist(2.0f, 8.0f);
    std::uniform_int_distribution<int> color_dist(0, 255);
    
    for (int s = 0; s < num_spheres; ++s) {
        float cx = pos_dist(rng);
        float cy = pos_dist(rng);
        float cz = pos_dist(rng);
        float radius = radius_dist(rng);
        
        uint8_t r = color_dist(rng);
        uint8_t g = color_dist(rng);
        uint8_t b = color_dist(rng);
        
        int points_per_sphere = 10000;
        for (int i = 0; i < points_per_sphere; ++i) {
            // Fibonacci sphere distribution
            float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / points_per_sphere);
            float theta = M_PI * (1.0f + std::sqrt(5.0f)) * i;
            
            float x = cx + radius * std::sin(phi) * std::cos(theta);
            float y = cy + radius * std::sin(phi) * std::sin(theta);
            float z = cz + radius * std::cos(phi);

            y += 10.0f;
            
            points.push_back({x, y, z, r, g, b, 0});
        }
    }
}

// Generate multiple point cloud spirals/helixes
void generateHelix(std::vector<Point>& points, int count, std::mt19937& rng) {
    std::uniform_real_distribution<float> pos_dist(-40.0f, 40.0f);
    std::uniform_real_distribution<float> radius_dist(8.0f, 15.0f);
    std::uniform_int_distribution<int> color_base_dist(0, 255);
    
    int num_helixes = 8;
    int points_per_helix = count / num_helixes;
    
    for (int h = 0; h < num_helixes; ++h) {
        // Random center position for each helix
        float center_x = pos_dist(rng);
        float center_z = pos_dist(rng);
        float base_radius = radius_dist(rng);
        
        // Random color scheme for each helix
        uint8_t color_offset_r = color_base_dist(rng);
        uint8_t color_offset_g = color_base_dist(rng);
        uint8_t color_offset_b = color_base_dist(rng);
        
        for (int i = 0; i < points_per_helix; ++i) {
            float t = i * 0.01f;
            float radius = base_radius + 3.0f * std::sin(t * 3.0f);
            
            // Spread around xz plane with individual centers
            float x = center_x + radius * std::cos(t * 2.0f);
            float z = center_z + radius * std::sin(t * 2.0f);
            
            // Much shorter height range (-15 to 15)
            float y = t * 0.6f - 15.0f;
            
            // Vary colors along the helix
            uint8_t r = static_cast<uint8_t>((std::sin(t + color_offset_r * 0.01f) * 0.5f + 0.5f) * 255);
            uint8_t g = static_cast<uint8_t>((std::cos(t + color_offset_g * 0.01f) * 0.5f + 0.5f) * 255);
            uint8_t b = static_cast<uint8_t>((std::sin(t * 0.5f + color_offset_b * 0.01f) * 0.5f + 0.5f) * 255);
            
            points.push_back({x, y, z, r, g, b, 0});
        }
    }
}

// Generate random scattered points
void generateRandom(std::vector<Point>& points, int count, std::mt19937& rng) {
    std::uniform_real_distribution<float> pos_dist(-50.0f, 50.0f);
    std::uniform_int_distribution<int> color_dist(0, 255);
    
    for (int i = 0; i < count; ++i) {
        float x = pos_dist(rng);
        float y = pos_dist(rng);
        float z = pos_dist(rng);
        
        uint8_t r = color_dist(rng);
        uint8_t g = color_dist(rng);
        uint8_t b = color_dist(rng);
        
        points.push_back({x, y, z, r, g, b, 0});
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int total_points = 10000000; // Default 10M points
    std::string output_file = "pointcloud.pcd";
    
    if (argc > 1) {
        total_points = std::atoi(argv[1]);
    }
    if (argc > 2) {
        output_file = argv[2];
    }
    
    std::cout << "Generating point cloud with " << total_points << " points..." << std::endl;
    
    // Random number generator
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // Generate points
    std::vector<Point> all_points;
    all_points.reserve(total_points);
    
    std::cout << "Generating terrain..." << std::endl;
    generateTerrain(all_points, total_points / 2, rng);
    
    std::cout << "Generating spheres..." << std::endl;
    generateSpheres(all_points, 8, rng); // Spheres contain many points
    
    std::cout << "Generating helix..." << std::endl;
    generateHelix(all_points, total_points / 2, rng);
    
    //std::cout << "Generating random scatter..." << std::endl;
    //generateRandom(all_points, total_points / 4, rng);
    
    std::cout << "Total points generated: " << all_points.size() << std::endl;
    
    // Calculate overall bounds
    BoundingBox bounds = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
    
    for (const auto& point : all_points) {
        bounds.min_x = std::min(bounds.min_x, point.x);
        bounds.min_y = std::min(bounds.min_y, point.y);
        bounds.min_z = std::min(bounds.min_z, point.z);
        bounds.max_x = std::max(bounds.max_x, point.x);
        bounds.max_y = std::max(bounds.max_y, point.y);
        bounds.max_z = std::max(bounds.max_z, point.z);
    }
    
    std::cout << "Bounds: (" << bounds.min_x << ", " << bounds.min_y << ", " << bounds.min_z 
              << ") to (" << bounds.max_x << ", " << bounds.max_y << ", " << bounds.max_z << ")" << std::endl;
    
    // Build octree for spatial organization
    std::cout << "Building octree..." << std::endl;
    OctreeNode root(bounds);
    int max_points_per_leaf = 100000; // 100k points per leaf
    int max_depth = 8;
    
    for (const auto& point : all_points) {
        root.insert(point, max_points_per_leaf, max_depth);
    }
    
    // Collect chunks from octree
    std::cout << "Collecting chunks..." << std::endl;
    std::vector<std::vector<Point>> chunks;
    root.collectChunks(chunks, 1000); // Minimum 1000 points per chunk
    
    std::cout << "Total chunks: " << chunks.size() << std::endl;
    
    // Prepare file header
    FileHeader header;
    std::memcpy(header.magic, "PCLOUD1", 8);
    header.version = 1;
    header.bounds = bounds;
    header.total_points = all_points.size();
    header.chunk_count = chunks.size();
    header.chunk_size = max_points_per_leaf;
    
    // Prepare chunk metadata
    std::vector<ChunkMetadata> chunk_metadata;
    chunk_metadata.reserve(chunks.size());
    
    // Calculate file offsets and chunk bounding boxes
    uint64_t current_offset = sizeof(FileHeader) + chunks.size() * sizeof(ChunkMetadata);
    
    for (const auto& chunk : chunks) {
        ChunkMetadata meta;
        meta.point_count = chunk.size();
        meta.file_offset = current_offset;
        
        // Calculate chunk bounds
        meta.bbox = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };
        
        for (const auto& point : chunk) {
            meta.bbox.min_x = std::min(meta.bbox.min_x, point.x);
            meta.bbox.min_y = std::min(meta.bbox.min_y, point.y);
            meta.bbox.min_z = std::min(meta.bbox.min_z, point.z);
            meta.bbox.max_x = std::max(meta.bbox.max_x, point.x);
            meta.bbox.max_y = std::max(meta.bbox.max_y, point.y);
            meta.bbox.max_z = std::max(meta.bbox.max_z, point.z);
        }
        
        chunk_metadata.push_back(meta);
        current_offset += chunk.size() * sizeof(Point);
    }
    
    // Write to file
    std::cout << "Writing to file: " << output_file << std::endl;
    std::ofstream file(output_file, std::ios::binary);
    
    if (!file) {
        std::cerr << "Failed to open output file!" << std::endl;
        return 1;
    }
    
    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));
    
    // Write chunk metadata
    file.write(reinterpret_cast<const char*>(chunk_metadata.data()), 
               chunk_metadata.size() * sizeof(ChunkMetadata));
    
    // Write chunk data
    for (size_t i = 0; i < chunks.size(); ++i) {
        file.write(reinterpret_cast<const char*>(chunks[i].data()), 
                   chunks[i].size() * sizeof(Point));
        
        if (i % 100 == 0) {
            std::cout << "Progress: " << (i * 100 / chunks.size()) << "%" << std::endl;
        }
    }
    
    file.close();
    
    // Print statistics
    uint64_t file_size = current_offset;
    std::cout << "\n=== Generation Complete ===" << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "File size: " << (file_size / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Total points: " << all_points.size() << std::endl;
    std::cout << "Total chunks: " << chunks.size() << std::endl;
    std::cout << "Avg points/chunk: " << (all_points.size() / chunks.size()) << std::endl;
    
    return 0;
}

