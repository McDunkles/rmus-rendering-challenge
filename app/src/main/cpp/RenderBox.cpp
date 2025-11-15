//
// Created by dunca on 2025-11-12.
//

#include "RenderBox.h"
#include "AndroidOut.h"


void RenderBox::setDims(int x, int y, int z) {
    bufferDims.x = x;
    bufferDims.y = y;
    bufferDims.z = z;

    totalSize = bufferDims.x*bufferDims.y*bufferDims.z;

    aout << "[RenderBox] dimX = " << bufferDims.x << ", dimY = " << bufferDims.y
    << ", dimZ = " << bufferDims.z << "; totalSize = " << totalSize << "\n";

    active_indices.reserve(totalSize);
    std::fill(active_indices.begin(), active_indices.end(), false);

    num_points_array.reserve(totalSize);
    std::fill(num_points_array.begin(), num_points_array.end(), 0);

    // pcd_buffer.reserve(totalSize);
    setBitMasks();
}

void RenderBox::setBitMasks() {

    int numLayers = 0;

    uint32_t bitMaskTemp = 0;

    for (int i = 0; i<bufferDims.length(); i++) {
        if (bufferDims[i] > 0) {
            numLayers = (int)(ceil(log(bufferDims[i])/log(2)));
        }

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