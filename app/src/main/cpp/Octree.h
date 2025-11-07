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
    BoundingBox bbox;
    std::unique_ptr<OctreeNode> children[8];
    int depth;
    int octantNum;
    bool is_leaf;

    explicit OctreeNode(const BoundingBox& box, int octant_num, int depth) :
    bbox(box), is_leaf(true), octantNum(octant_num), depth(depth) {
        for (int i = 0; i < 8; ++i) {
            children[i] = nullptr;
        }
    }

    void insert(const BoundingBox& box);

    int getOctant(float x, float y, float z);

    OctreeNode *getNode(glm::vec3 point);

    /*
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

        is_leaf = false;
    }
    */

    /*
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
    */
};


#endif //RENDERINGCHALLENGE_OCTREE_H
