#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include "../app/src/main/cpp/PointCloudData.h"

void printHeader(const FileHeader& header) {
    std::cout << "\n=== Point Cloud File Info ===" << std::endl;
    std::cout << "Magic: " << std::string(header.magic, 7) << std::endl;
    std::cout << "Version: " << header.version << std::endl;
    std::cout << "Total Points: " << header.total_points << std::endl;
    std::cout << "Chunk Count: " << header.chunk_count << std::endl;
    std::cout << "Target Chunk Size: " << header.chunk_size << std::endl;
    
    std::cout << "\nBounding Box:" << std::endl;
    std::cout << "  Min: (" << header.bounds.min_x << ", " 
              << header.bounds.min_y << ", " << header.bounds.min_z << ")" << std::endl;
    std::cout << "  Max: (" << header.bounds.max_x << ", " 
              << header.bounds.max_y << ", " << header.bounds.max_z << ")" << std::endl;
    
    float cx, cy, cz;
    header.bounds.getCenter(cx, cy, cz);
    std::cout << "  Center: (" << cx << ", " << cy << ", " << cz << ")" << std::endl;
    std::cout << "  Size: " << header.bounds.size() << std::endl;
}

void printChunkStats(const std::vector<ChunkMetadata>& chunks) {
    std::cout << "\n=== Chunk Statistics ===" << std::endl;
    
    if (chunks.empty()) {
        std::cout << "No chunks found!" << std::endl;
        return;
    }
    
    uint32_t min_points = chunks[0].point_count;
    uint32_t max_points = chunks[0].point_count;
    uint64_t total_points = 0;
    float min_size = chunks[0].bbox.size();
    float max_size = chunks[0].bbox.size();
    
    for (const auto& chunk : chunks) {
        min_points = std::min(min_points, chunk.point_count);
        max_points = std::max(max_points, chunk.point_count);
        total_points += chunk.point_count;
        
        float size = chunk.bbox.size();
        min_size = std::min(min_size, size);
        max_size = std::max(max_size, size);
    }
    
    std::cout << "Total Chunks: " << chunks.size() << std::endl;
    std::cout << "Points per Chunk:" << std::endl;
    std::cout << "  Min: " << min_points << std::endl;
    std::cout << "  Max: " << max_points << std::endl;
    std::cout << "  Avg: " << (total_points / chunks.size()) << std::endl;
    
    std::cout << "Chunk Size (spatial):" << std::endl;
    std::cout << "  Min: " << min_size << std::endl;
    std::cout << "  Max: " << max_size << std::endl;
    std::cout << "  Avg: " << ((min_size + max_size) / 2.0f) << std::endl;
}

void printDetailedChunks(const std::vector<ChunkMetadata>& chunks, int max_display = 10) {
    std::cout << "\n=== Chunk Details (first " << max_display << ") ===" << std::endl;
    std::cout << std::setw(5) << "ID" 
              << std::setw(12) << "Points"
              << std::setw(15) << "Offset"
              << std::setw(35) << "Bounding Box"
              << std::endl;
    std::cout << std::string(67, '-') << std::endl;
    
    for (size_t i = 0; i < chunks.size() && i < max_display; ++i) {
        const auto& chunk = chunks[i];
        float cx, cy, cz;
        chunk.bbox.getCenter(cx, cy, cz);
        
        std::cout << std::setw(5) << i
                  << std::setw(12) << chunk.point_count
                  << std::setw(15) << chunk.file_offset
                  << "  [" << std::fixed << std::setprecision(1)
                  << cx << ", " << cy << ", " << cz << "]"
                  << std::endl;
    }
    
    if (chunks.size() > max_display) {
        std::cout << "... (" << (chunks.size() - max_display) << " more chunks)" << std::endl;
    }
}

void printMemoryEstimate(const FileHeader& header, const std::vector<ChunkMetadata>& chunks) {
    std::cout << "\n=== Memory Estimates ===" << std::endl;
    
    uint64_t total_file_size = sizeof(FileHeader) + 
                               chunks.size() * sizeof(ChunkMetadata) +
                               header.total_points * sizeof(Point);
    
    uint64_t index_size = sizeof(FileHeader) + chunks.size() * sizeof(ChunkMetadata);
    uint64_t point_data_size = header.total_points * sizeof(Point);
    
    std::cout << "File Structure:" << std::endl;
    std::cout << "  Header: " << sizeof(FileHeader) << " bytes" << std::endl;
    std::cout << "  Index: " << (chunks.size() * sizeof(ChunkMetadata) / 1024) << " KB" << std::endl;
    std::cout << "  Point Data: " << (point_data_size / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Total File: " << (total_file_size / 1024 / 1024) << " MB" << std::endl;
    
    std::cout << "\nRuntime Memory (if all loaded):" << std::endl;
    std::cout << "  CPU: ~" << (point_data_size / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  GPU (VBOs): ~" << (point_data_size / 1024 / 1024) << " MB" << std::endl;
    
    // Estimate streaming memory with typical cache
    uint64_t cache_size_mb = 100; // Assume 100MB cache
    uint64_t chunks_in_cache = (cache_size_mb * 1024 * 1024) / (chunks[0].point_count * sizeof(Point));
    
    std::cout << "\nStreaming Mode (100 MB cache):" << std::endl;
    std::cout << "  ~" << chunks_in_cache << " chunks in memory" << std::endl;
    std::cout << "  ~" << (chunks_in_cache * chunks[0].point_count) << " points loaded" << std::endl;
    std::cout << "  ~" << cache_size_mb << " MB CPU + " << cache_size_mb << " MB GPU" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pointcloud_file> [--detailed]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    bool detailed = (argc > 2 && std::string(argv[2]) == "--detailed");
    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }
    
    // Read header
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
    
    if (std::string(header.magic, 7) != "PCLOUD1") {
        std::cerr << "Invalid file format! Magic: " << std::string(header.magic, 7) << std::endl;
        return 1;
    }
    
    // Read chunk metadata
    std::vector<ChunkMetadata> chunks(header.chunk_count);
    file.read(reinterpret_cast<char*>(chunks.data()), 
              header.chunk_count * sizeof(ChunkMetadata));
    
    file.close();
    
    // Print information
    printHeader(header);
    printChunkStats(chunks);
    
    if (detailed) {
        printDetailedChunks(chunks, 20);
    } else {
        printDetailedChunks(chunks, 10);
    }
    
    printMemoryEstimate(header, chunks);
    
    std::cout << "\n=== End of Report ===" << std::endl;
    
    return 0;
}

