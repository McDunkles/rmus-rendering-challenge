//
// Created by dunca on 2025-11-12.
//

#ifndef RENDERINGCHALLENGE_RENDERBOX_H
#define RENDERINGCHALLENGE_RENDERBOX_H

#include <vector>

#include "../../../../tools/PointCloudData.h"
#include "glm/vec3.hpp"

using cpoint_t = struct Point;

class RenderBox {

public:

    static constexpr uint16_t MAX_CAPACITY = 32;

    // int dimX, dimY, dimZ;

    glm::vec<3, int, glm::defaultp> bufferDims = {0, 0, 0};

    int totalSize = 0;
    int chunk_size = 0;

    // uint32_t bitMaskX = 0;
    // uint32_t bitMaskY = 0;
    // uint32_t bitMaskZ = 0;

    glm::vec<3, uint32_t, glm::defaultp> bitMasks = {0, 0, 0};

    // Thinking about using an inner posCode
    // Minimum dimensions would be 2x2x2, equivalent to octree of depth 1
    // Really this could basically just be an octree structure
    // iPosCode = posCode & bitMask (maybe ?)
    // Keeps a total order, preserves spatial locality
    // std::vector<std::vector<cpoint_t>> pcd_buffer;
    std::vector<cpoint_t> pcd_buffer;
    std::vector<bool> active_indices;
    std::vector<int> num_points_array;

    glm::vec3 posBL;
    uint32_t posCodeBL;

    glm::vec3 posTR;
    uint32_t posCodeTR;


    RenderBox() = default;

    RenderBox(int dim_x, int dim_y, int dim_z) {

        bufferDims.x = dim_x;
        bufferDims.y = dim_y;
        bufferDims.z = dim_z;
        totalSize = dim_x*dim_y*dim_z;
    }

    void setDims(int x, int y, int z);

    void initBuffer(int chunkSize);

    void setPointCorners(glm::vec3 bl, glm::vec3 tr);

    void setPosCodes(uint32_t pc_bl, uint32_t pc_tr);


private:
    void setBitMasks();
};


#endif //RENDERINGCHALLENGE_RENDERBOX_H
