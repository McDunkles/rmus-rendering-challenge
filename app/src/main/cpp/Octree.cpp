//
// Created by dunca on 2025-11-07.
//

#include "Octree.h"
#include "AndroidOut.h"


int OctreeNode::getOctant(float x, float y, float z) {
    float mid_x = (bbox.min_x + bbox.max_x) / 2.0f;
    float mid_y = (bbox.min_y + bbox.max_y) / 2.0f;
    float mid_z = (bbox.min_z + bbox.max_z) / 2.0f;

    int octant = 0;
    if (x >= mid_x) octant |= 1;
    if (y >= mid_y) octant |= 2;
    if (z >= mid_z) octant |= 4;

    return octant;
}


void OctreeNode::insert(const BoundingBox& box) {
    glm::vec3 center;
    box.getCenter(center.x, center.y, center.z);

    int octant = getOctant(center.x, center.y, center.z);

    if (children[octant] == nullptr) {
        children[octant] = std::make_unique<OctreeNode>(box, octant, depth+1);
        this->is_leaf = false;
    } else {
        children[octant]->insert(box);
    }
}


OctreeNode *OctreeNode::getNode(glm::vec3 point) {

    int octant = getOctant(point.x, point.y, point.z);

    if (children[octant] == nullptr) {
        aout << "OctreeNode child node is null. Something has gone very wrong. " <<
                "This should not be possible.\n";
    } else if (children[octant]->is_leaf) {
        return children[octant].get();
    } else {
        return children[octant]->getNode(point);
    }

    return nullptr;
}