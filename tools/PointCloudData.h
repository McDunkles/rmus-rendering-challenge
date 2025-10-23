#ifndef POINTCLOUDDATA_H
#define POINTCLOUDDATA_H

#include <cstdint>

struct Point {
    float x, y, z;
    uint8_t r, g, b;
    uint8_t padding;
};

struct BoundingBox {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
    
    [[nodiscard]] inline bool contains(float x, float y, float z) const {
        return x >= min_x && x <= max_x &&
               y >= min_y && y <= max_y &&
               z >= min_z && z <= max_z;
    }
    
    [[nodiscard]] inline float size() const {
        float dx = max_x - min_x;
        float dy = max_y - min_y;
        float dz = max_z - min_z;
        return dx > dy ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
    }
    
    [[nodiscard]] inline float maxDimension() const {
        float dx = max_x - min_x;
        float dy = max_y - min_y;
        float dz = max_z - min_z;
        return dx > dy ? (dx > dz ? dx : dz) : (dy > dz ? dy : dz);
    }
    
    inline void getCenter(float& cx, float& cy, float& cz) const {
        cx = (min_x + max_x) * 0.5f;
        cy = (min_y + max_y) * 0.5f;
        cz = (min_z + max_z) * 0.5f;
    }
};

struct ChunkMetadata {
    BoundingBox bbox;
    uint32_t point_count;
    uint64_t file_offset;
};

struct FileHeader {
    char magic[8];          // "PCLOUD1\0"
    uint32_t version;       // Format version
    BoundingBox bounds;     // Overall bounds
    uint64_t total_points;  // Total number of points
    uint32_t chunk_count;   // Number of chunks
    uint32_t chunk_size;    // Target points per chunk
};

#endif //POINTCLOUDDATA_H
