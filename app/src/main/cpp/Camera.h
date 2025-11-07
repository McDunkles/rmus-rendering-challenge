//
// Created by dunca on 2025-11-04.
//

#ifndef RENDERINGCHALLENGE_CAMERA_H
#define RENDERINGCHALLENGE_CAMERA_H

// #include <EGL/egl.h>
// #include <GLES3/gl3.h>

#include <EGL/egl.h>
#include "glm/glm.hpp"

class Camera {

public:

    glm::vec3 pos_;
    glm::vec3 target_;
    glm::vec3 up_;
    glm::mat4 viewMatrix_;

    explicit inline Camera() : pos_(glm::vec3(0.0f)),
    target_(glm::vec3(0.0f)), up_(glm::vec3(0.0f)),
    viewMatrix_(glm::mat4(0.0f)) {

    }

    explicit inline Camera(glm::vec3 pos, glm::vec3 target, glm::vec3 up)
    : pos_(pos), target_(target), up_(up), viewMatrix_(glm::mat4(0.0f)) {
        initCamera();
    }

    void initCamera();

    void updateViewMatrix();

};


#endif //RENDERINGCHALLENGE_CAMERA_H
