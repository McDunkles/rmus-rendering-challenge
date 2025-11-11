//
// Created by dunca on 2025-11-07.
//

#include "Octree.h"
#include "AndroidOut.h"

#include <sstream>
#include <iterator>
#include <cstring>
#include <cstdio>


int OctreeNode::getMaxDepth(OctreeNode *node) {
    int nDepth = 0;

    if (node != nullptr) {

        nDepth = node->depth;

        if (!node->is_leaf) {
            for (int i = 0; i < std::size(node->children); i++) {
                nDepth =
                std::max(nDepth,
                 getMaxDepth(node->children[i].get()));
            }
        }

    }

    return nDepth;
}

/*
 * Currently assigns:
 * encodedPosition
 */
void OctreeNode::assignAuxInfo(OctreeNode *node, int maxDepth) {

    if (node != nullptr) {

        if (node->is_leaf) {

            uint32_t bitMask = 0b111;
            for (int i = 0; i<node->lineage.size(); i++) {
                node->encodedPosition |=
                (node->lineage[i] & bitMask) << (3*(maxDepth - i - 1));
            }

            node->encodedPosition |=
                    (node->octantNum & bitMask) << (3*(maxDepth - node->depth));

        } else {
            for (int i = 0; i < std::size(node->children); i++) {
                assignAuxInfo(node->children[i].get(), maxDepth);
            }
        }
    }

}


void OctreeNode::assignChunkMetadata(std::vector<ChunkMetadata> chunkData, int maxDepth) {

    for (int i = 0; i<chunkData.size(); i++) {
        glm::vec3 chunkCenter;
        chunkData[i].bbox.getCenter(chunkCenter.x, chunkCenter.y, chunkCenter.z);

        uint32_t posCode = getPosCode(chunkCenter, maxDepth);

        OctreeNode *node = getNodeSoft(posCode, maxDepth);

        node->byteOffset = chunkData[i].file_offset;
        node->numPoints = chunkData[i].point_count;
    }

}


void OctreeNode::printNode() {
    aout << "Depth = " << this->depth << "; Octant = " << this->octantNum
         << "; posCode = " << this->encodedPosition <<
        "; ByteOffset = " << byteOffset <<
        "; NumPoints = " << numPoints <<
        "\n";
}


void OctreeNode::printTreeLeaves(OctreeNode *root) {

    if (root != nullptr) {

        if (root->is_leaf) {

            std::string pStr2;

            // std::sprintf(pStr2.data(), "Depth = %d; Octant = %d; posCode = %u",
                         // root->depth, root->octantNum, root->encodedPosition);

            aout << "Depth = " << root->depth << "; Octant = " << root->octantNum
            << "; posCode = " << root->encodedPosition <<
            "; ByteOffset = " << root->byteOffset <<
            "; NumPoints = " << root->numPoints <<
            "\n";
        } else {

            for (int i = 0; i<std::size(root->children); i++) {
                root->printTreeLeaves(root->children[i].get());
            }

        }

    }

}


void OctreeNode::insert(const BoundingBox& box, BoundingBox absolute_bounds) {
    glm::vec3 center;
    box.getCenter(center.x, center.y, center.z);

    int octant = getOctant(center.x, center.y, center.z);

    // aout << "[insert] depth = " << depth << "; octant = " << octant << "\n";

    if (children[octant] == nullptr) {

        int est_depth = getBBDepth(box, absolute_bounds);
        // int est_depth = this->depth;

        if (est_depth == this->depth+1) {

            children[octant] = std::make_unique<OctreeNode>(
                    box, octant, depth+1, this);
        } else {

            // aout << "This puny box of depth " << est_depth << " is unworthy !!!\n";

            // We must craft a box worthy of bestowing upon the new child
            BoundingBox worthyBox = subdivide(octant);
            children[octant] = std::make_unique<OctreeNode>(
                    worthyBox, octant, depth+1, this);

            /*
            aout << "BoxBounds: x = (" << worthyBox.min_x << ", " << worthyBox.max_x
            << "); y = (" << worthyBox.min_y << ", " << worthyBox.max_y << ")" <<
            "; z = (" << worthyBox.min_z << ", " << worthyBox.max_z << ")\n";

            int oct2 = children[octant]->getOctant(-5, -30, -25);

            aout << "Da octant = " << oct2 << "\n";
            */

            // The box does not fit the child, so it becomes a
            // hand-me-down. Many such cases.
            children[octant]->insert(box, absolute_bounds);
        }


        this->is_leaf = false;
    } else {
        children[octant]->insert(box, absolute_bounds);
    }
}


int OctreeNode::getOctant(float x, float y, float z, bool pInfo) {

    if (pInfo) {
        aout << "[getOctant] BoundingBox: x = ("
             << bbox.min_x << ", " << bbox.max_x
             << "); y = (" << bbox.min_y << ", " << bbox.max_y << ")" <<
             "; z = (" << bbox.min_z << ", " << bbox.max_z << ")\n";


        aout << "[getOctant] Point = (" << x << ", " << y << ", "
             << z << ")\n";
    }


    float mid_x = (bbox.min_x + bbox.max_x) / 2.0f;
    float mid_y = (bbox.min_y + bbox.max_y) / 2.0f;
    float mid_z = (bbox.min_z + bbox.max_z) / 2.0f;

    int octant = 0;
    if (x >= mid_x) octant |= 1;
    if (y >= mid_y) octant |= 2;
    if (z >= mid_z) octant |= 4;

    return octant;
}


BoundingBox OctreeNode::subdivide(int octant) {
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



    is_leaf = false;

    return child_boxes[octant];
}

// Also consider: getClosestNode(uint32_t posCode, int maxDepth)
// For cases such as returning depth 1 leaf 010 000 even if you
// searched for a leaf of depth 2 w/ code 010 101 (which wouldn't
// exist if 010 000 at depth 1 is a leaf)

// Returns null if no node with the specified posCode exists
OctreeNode *OctreeNode::getNode(uint32_t posCode, int maxDepth) {

    if (is_leaf) {
        if (encodedPosition == posCode) {
            return this;
        } else {
            return nullptr;
        }
    } else {

        int bitOffset = (maxDepth - (depth + 1));
        uint32_t bitMask = 0b111;

        uint32_t octant = posCode & (bitMask << (3*bitOffset));
        octant = octant >> (3*bitOffset);

        if (children[octant] == nullptr) {
            return nullptr;
        } else {
            return children[octant]->getNode(posCode, maxDepth);
        }
    }

}

OctreeNode *OctreeNode::getNodeSoft(uint32_t posCode, int maxDepth) {
    if (is_leaf) {

        int depthDiff = maxDepth - depth;
        uint32_t bitMask = ~((1 << (3*depthDiff)) - 1);
        uint32_t posCodeAdj = posCode & bitMask;

        if (encodedPosition == posCodeAdj) {
            return this;
        } else {
            return nullptr;
        }
    } else {

        int bitOffset = (maxDepth - (depth + 1));
        uint32_t bitMask = 0b111;

        uint32_t octant = posCode & (bitMask << (3*bitOffset));
        octant = octant >> (3*bitOffset);

        if (children[octant] == nullptr) {
            return nullptr;
        } else {
            return children[octant]->getNodeSoft(posCode, maxDepth);
        }
    }
}


// Must be called on by the root node, which has the full bounding box
uint32_t OctreeNode::getPosCode(glm::vec3 point, int maxDepth) {

    uint32_t posCode = 0;
    uint32_t bitMask = 0b111;

    OctreeNode *node = this;

    for (int i = 0; i<maxDepth; i++) {
        int octant = node->getOctant(point.x, point.y, point.z);

        if (node->children[octant] == nullptr) {
            break;
        } else {

            posCode |= (octant & bitMask) << (3*(maxDepth - i - 1));
            node = node->children[octant].get();
        }
    }

    return posCode;
}


uint32_t OctreeNode::getPosCodeExact(glm::vec3 point, BoundingBox absoluteBounds, int maxDepth) {

    uint32_t posCode = 0;

    auto numSlices = (float)exp2(maxDepth);

    glm::vec3 unitBox;
    unitBox.x = (absoluteBounds.max_x - absoluteBounds.min_x) / numSlices;
    unitBox.y = (absoluteBounds.max_y - absoluteBounds.min_y) / numSlices;
    unitBox.z = (absoluteBounds.max_z - absoluteBounds.min_z) / numSlices;

    auto unitsX = (uint32_t)floor((point.x - absoluteBounds.min_x)/unitBox.x);
    auto unitsY = (uint32_t)floor((point.y - absoluteBounds.min_y)/unitBox.y);
    auto unitsZ = (uint32_t)floor((point.z - absoluteBounds.min_z)/unitBox.z);

    // aout << "getPCE() :: (ux, uy, uz) = (" << unitsX << ", " << unitsY
    // << ", " << unitsZ << ")\n";

    for (int i = 0; i<maxDepth; i++) {

        uint32_t bitX = unitsX & ( 1 << (maxDepth - i - 1) );
        uint32_t bitY = unitsY & ( 1 << (maxDepth - i - 1) );
        uint32_t bitZ = unitsZ & ( 1 << (maxDepth - i - 1) );

        uint32_t octant = (bitZ << 2) | (bitY << 1) | bitX;

        posCode |= (octant) << ( 2*(maxDepth - i - 1) );

    }

    return posCode;
}



OctreeNode *OctreeNode::getNode(glm::vec3 point) {

    // aout << "[Octree::getNode] (depth, octNum) = ("
    // << depth << ", " << octantNum << ")\n";

    // glm::vec3 bc;
    // bbox.getCenter(bc.x, bc.y, bc.z);

    // aout << "bbox = (" << bc.x << ", " << bc.y << ", " << bc.z << ")\n";
    int octant = getOctant(point.x, point.y, point.z);

    // aout << "Resulting octant = " << octant << "\n";

    if (children[octant] == nullptr) {
        aout << "OctreeNode child node is null. There are no chunks that cover this area.\n";
    } else if (children[octant]->is_leaf) {
        return children[octant].get();
    } else {
        return children[octant]->getNode(point);
    }

    return nullptr;
}


std::string OctreeNode::getLineageStr() {
    std::stringstream linStr;

    linStr << "OctreeLineage{";

    for (int num : lineage) {
        linStr << num << " ";
    }
    linStr << "}";

    return linStr.str();
}


int OctreeNode::getBBDepth(const BoundingBox& bb, BoundingBox absolute_bounds) {


    float total_span_x = absolute_bounds.max_x - absolute_bounds.min_x;
    float total_span_y = absolute_bounds.max_y - absolute_bounds.min_y;
    float total_span_z = absolute_bounds.max_z - absolute_bounds.min_z;

    float bb_span_x = bb.max_x - bb.min_x;
    float bb_span_y = bb.max_y - bb.min_y;
    float bb_span_z = bb.max_z - bb.min_z;

    float x_ratio = total_span_x/bb_span_x;
    float y_ratio = total_span_y/bb_span_y;
    float z_ratio = total_span_z/bb_span_z;

    float min_ratio = std::min({x_ratio, y_ratio, z_ratio});

    int assigned_depth = (int)std::floor( std::log(min_ratio)/std::log(2.f) );

    return assigned_depth;
}