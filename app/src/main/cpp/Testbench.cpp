//
// Created by dunca on 2025-11-11.
//

#include "Testbench.h"

#include "AndroidOut.h"

#include "../../../../tools/PointCloudData.h"
#include "Octree.h"

#include <vector>


void Testbench::testOctreeFuncs() {

    aout << "[testOctreeFuncs] Starting Method\n";

    std::string internal_path("/data/user/0/com.rmus.renderingchallenge/files/pointcloud_10m.pcd");
    pcd_file.open(internal_path,std::ios::binary | std::ios::in);

    FileHeader header;
    pcd_file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));

    int chunk_count = header.chunk_count;
    aout << TB_AOUT_SIG <<
    "Initializing dataset... there are [" << chunk_count << "] chunks in the data\n";

    std::vector<ChunkMetadata> chunkData(chunk_count);
    pcd_file.read(reinterpret_cast<char*>(chunkData.data()), sizeof(ChunkMetadata) * chunk_count);

    aout << TB_AOUT_SIG <<
    "Chunk metadata read in... ready to start loading in point cloud data!\n";

    BoundingBox absoluteBounds = header.bounds;
    OctreeNode *root = new OctreeNode(absoluteBounds, 0, 0);

    for (int i = 0; i<chunk_count; i++) {
        root->insert(chunkData[i].bbox, absoluteBounds);
    }

    int maxDepth = root->getMaxDepth(root);
    aout << TB_AOUT_SIG <<
    "[INIT DATA] maxDepth = " << maxDepth << "\n";

    glm::vec3 target = {-15, -15, -11.5};

    root->assignAuxInfo(root, maxDepth);
    root->assignChunkMetadata(chunkData, maxDepth);

    uint32_t posCode = root->getPosCode(target, maxDepth);
    aout << TB_AOUT_SIG << "posCode = " << posCode << "\n";

    OctreeNode *desiredNode = root->getNode(posCode, maxDepth);
    BoundingBox bbox1 = desiredNode->bbox;
}
