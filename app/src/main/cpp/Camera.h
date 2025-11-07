//
// Created by dunca on 2025-11-04.
//

#ifndef RENDERINGCHALLENGE_CAMERA_H
#define RENDERINGCHALLENGE_CAMERA_H

// #include <EGL/egl.h>
// #include <GLES3/gl3.h>

#include <EGL/egl.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Camera {

public:

    static constexpr float zNear = 0.5f;
    static constexpr float zFar = 50.f;

    glm::vec3 pos_;
    glm::vec3 target_;
    glm::vec3 up_;
    glm::mat4 viewMatrix_;
    float aspectRatio = 1;
    float fovy = glm::pi<float>()/3.0f;
    float distScalarY;
    float targetDist;

    glm::vec2 tiltBuffer;
    int tiltCount;
    int tiltThreshold;

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


    // Assuming a perspective view, this method calculates
    // the x-y bounds at the near plane, far plane, and
    // at the target.
    void calcDistScalar();


    void camTilt(glm::vec2 tiltDir);

    void camPan(glm::vec2 panDir);

};


#endif //RENDERINGCHALLENGE_CAMERA_H
