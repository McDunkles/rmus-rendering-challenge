//
// Created by dunca on 2025-11-07.
//

#ifndef RENDERINGCHALLENGE_OCTREE_H
#define RENDERINGCHALLENGE_OCTREE_H

#include <vector>
#include "glm/glm.hpp"

#include "../../../../tools/PointCloudData.h"

// Octree node for spatial organization
struct OctreeNode {
    // static BoundingBox absolute_bounds;
    BoundingBox bbox;
    std::unique_ptr<OctreeNode> children[8];
    OctreeNode *parent;
    int depth;
    int octantNum;
    bool is_leaf;
    std::vector<int> lineage;
    uint32_t encodedPosition;

    // Put these in aux info
    uint64_t byteOffset;
    uint32_t numPoints;

    glm::vec3 getDisplacement(OctreeNode *node1, OctreeNode *node2);

    int getDistance(OctreeNode *node1, OctreeNode *node2);

    // Must be called on by the root node, which has the full bounding box
    uint32_t getPosCode(glm::vec3 point, int maxDepth);

    uint32_t getPosCodeExact(glm::vec3 point, BoundingBox absoluteBounds, int maxDepth);

    void printTreeLeaves(OctreeNode *root);

    void printNode();

    void assignAuxInfo(OctreeNode *node, int maxDepth);

    void assignChunkMetadata(std::vector<ChunkMetadata> chunkData, int maxDepth);

    int getMaxDepth(OctreeNode *node);

    explicit OctreeNode(const BoundingBox& box, int octant_num,
                        int depth, OctreeNode *parent = nullptr) :
    bbox(box), is_leaf(true), octantNum(octant_num), depth(depth) {

        encodedPosition = 0;
        /*
        // Root node
        if (depth == 0) {

            // Create an absolute boundary that every node can see
            absolute_bounds = {
                    box.min_x, box.max_x,
                    box.min_y, box.max_y,
                    box.min_z, box.max_z
            };
        }
        */

        lineage.reserve(depth);

        if (parent != nullptr) {
            lineage.assign(parent->lineage.begin(), parent->lineage.end());
            lineage.push_back(octant_num);
        }

        for (int i = 0; i < 8; ++i) {
            children[i] = nullptr;
        }
    }

    void insert(const BoundingBox& box, BoundingBox absolute_bounds);

    int getOctant(float x, float y, float z);

    BoundingBox subdivide(int octant);

    OctreeNode *getNode(glm::vec3 point);

    OctreeNode *getNode(uint32_t posCode, int maxDepth);

    OctreeNode *getNodeSoft(uint32_t posCode, int maxDepth);

    std::string getLineageStr();

    int getBBDepth(const BoundingBox& bb, BoundingBox absolute_bounds);

};


#endif //RENDERINGCHALLENGE_OCTREE_H
