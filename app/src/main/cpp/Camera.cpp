//
// Created by dunca on 2025-11-04.
//

#include "Camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "AndroidOut.h"

#include <GLES3/gl3.h>


void Camera::initCamera() {

    // Create the view matrix
    glm::mat4 viewMatrix = glm::lookAt(pos_, target_, up_);

    viewMatrix_ = viewMatrix;
    targetDist = glm::length(target_ - pos_);

    tiltBuffer = {0, 0};
    tiltCount = 0;
    tiltThreshold = 3;

    aout << "Camera created! Target Distance = " << targetDist;
}

void Camera::updateViewMatrix() {
    // Create the view matrix
    glm::mat4 viewMatrix = glm::lookAt(pos_, target_, up_);

    viewMatrix_ = viewMatrix;
}


void Camera::calcDistScalar() {
    // Calculate near bounds, far bounds, and bounds at the target

    // First, get phi as an angle of theta (fovy)
    float phi = glm::asin(aspectRatio * glm::sin(fovy/2));
    distScalarY = 2*glm::tan(fovy/2)/glm::cos(phi);

}


void Camera::camTilt(glm::vec2 tiltDir) {

    tiltBuffer += tiltDir;

    if (((++tiltCount) % tiltThreshold) == 0) {

        // aout << "[CAMERA] CamTilt starting!\n";
        aout << "[CAMERA] tiltBuffer = (" << tiltBuffer[0] << ", "
        << tiltBuffer[1] << ")\n";

        glm::vec3 tgtDiff = target_ - pos_;
        float rho = glm::length(tgtDiff);

        float theta = glm::dot(tgtDiff, {1, 0, 0});
        theta = acos(theta / rho) + ((tgtDiff[1] < 0) ? glm::pi<float>() : 0);

        float phi = glm::dot(tgtDiff, {0, 0, 1});
        phi = acos(phi / rho);

        float diffTheta = 3*(glm::pi<float>()) * tiltDir[0];
        float diffPhi = 3*(glm::pi<float>()) * tiltDir[1];

        aout << "(rho, theta, phi) = (" << rho << ", "
             << theta << ", " << phi << ")\n";

        aout << "(diffTheta, diffPhi) = ("
             << diffTheta << ", " << diffPhi << ")\n";

        theta += diffTheta;
        phi += diffPhi;

        float newX = rho * glm::cos(theta) * glm::sin(phi);
        float newY = rho * glm::sin(theta) * glm::sin(phi);
        float newZ = rho * glm::cos(phi);

        float pChangeX = (newX - tgtDiff[0]) / tgtDiff[0];
        float pChangeY = (newY - tgtDiff[1]) / tgtDiff[1];
        float pChangeZ = (newZ - tgtDiff[2]) / tgtDiff[2];

        if ((pChangeX > 0.05) || (pChangeY > 0.05) || (pChangeZ > 0.05)) {
            aout << "[CAMERA] WARNING: Unexpectedly large tilt occurred: "
                 << "(xChg%, yChg%, zChg%) = (" << (pChangeX) << ", "
                 << (pChangeY) << ", " << (pChangeZ) << ")\n";
        }

        float diffX = newX - tgtDiff[0];
        float diffY = newY - tgtDiff[1];
        float diffZ = newZ - tgtDiff[2];

        // aout << "DIFF (x, y, z) = (" << (diffX) << ", "
        //      << (diffY) << ", " << (diffZ) << ")\n";


        pos_ = target_ - glm::vec3(newX, newY, newZ);
        // pos_ += glm::vec3(diffX, diffY, diffZ);

        // Reset the tilt buffer
        tiltBuffer = {0, 0};
        tiltCount = 0;

        aout << "[CAMERA] New Pos = (" << pos_[0] << ", " << pos_[1]
        << ", " << pos_[2] << ")\n";
    }
}

