//
// Created by dunca on 2025-11-10.
//

#ifndef RENDERINGCHALLENGE_OCTREEDATA_H
#define RENDERINGCHALLENGE_OCTREEDATA_H

#include "glm/glm.hpp"

#include "Octree.h"

#include "../../../../tools/PointCloudData.h"

class OctreeData {

public:
    OctreeNode *root;
    BoundingBox absoluteBounds;

    glm::vec3 unitBoxDims;
    int maxDepth;

};


#endif //RENDERINGCHALLENGE_OCTREEDATA_H
