//
// Created by dunca on 2025-11-04.
//

#include "Camera.h"
#include "glm/ext/matrix_transform.hpp"

#include <GLES3/gl3.h>


void Camera::initCamera() {

    // Create the view matrix
    glm::mat4 viewMatrix = glm::lookAt(pos_, target_, up_);

    viewMatrix_ = viewMatrix;
}

