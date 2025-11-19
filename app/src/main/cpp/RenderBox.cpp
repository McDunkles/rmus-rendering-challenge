//
// Created by dunca on 2025-11-12.
//

#include "RenderBox.h"
#include "AndroidOut.h"

void RenderBox::setDims(float camX, float camY, float camZ, glm::vec3 unitBox) {

    float CTC_len = sqrt(pow(camX, 2) + pow(camY, 2) + pow(camZ, 2));
    cubeSideLength = CTC_len;

    float ratio_x = CTC_len/(float)unitBox.x;
    float ratio_y = CTC_len/(float)unitBox.y;
    float ratio_z = CTC_len/(float)unitBox.z;

    int rbX = (int)ceil(ratio_x)+1;
    int rbY = (int)ceil(ratio_y)+1;
    int rbZ = (int)ceil(ratio_z)+1;

    bufferDims.x = rbX;
    bufferDims.y = rbY;
    bufferDims.z = rbZ;

    totalCubeSize = rbX * rbY * rbZ;

    // Old
    totalSize = bufferDims.x*bufferDims.y*bufferDims.z;

    aout << "[RenderBox] ratio_x = " << ratio_x << "; ratio_y = " << ratio_y << "; ratio_z = " << ratio_z << "\n";
    aout << "[RenderBox] Total Cube Size = " << totalCubeSize << "\n";

    aout << "[RenderBox] dimX = " << bufferDims.x << ", dimY = " << bufferDims.y
         << ", dimZ = " << bufferDims.z << "; totalSize = " << totalSize << "\n";

    aout << "[setDims] Total Cube Volume = " << totalCubeSize << "\n";

    active_indices.reserve(totalCubeSize);
    std::fill(active_indices.begin(), active_indices.end(), false);

    num_points_array.reserve(totalCubeSize);
    std::fill(num_points_array.begin(), num_points_array.end(), 0);

    // pcd_buffer.reserve(totalSize);
    setBitMasks();
}


void RenderBox::setBitMasks() {

    int numLayers = (int)(ceil(log(cubeSideLength)/log(2)));

    uint32_t bitMaskTemp = 0;

    for (int i = 0; i<bufferDims.length(); i++) {

        bitMaskTemp = (1 << i);

        for (int j = 0; j<numLayers; j++) {
            bitMasks[i] |= (bitMaskTemp << (3*j));
        }
    }
}


void RenderBox::initBuffer(int chunkSize) {

    pcd_buffer.reserve(totalSize * chunkSize);
    chunk_size = chunkSize;

}


void RenderBox::setPointCorners(glm::vec3 bl, glm::vec3 tr) {
    posBL = bl;
    posTR = tr;
}


void RenderBox::setPosCodes(uint32_t pc_bl, uint32_t pc_tr) {
    posCodeBL = pc_bl;
    posCodeTR = pc_tr;
}