//
// Created by dunca on 2025-11-06.
//

#ifndef RENDERINGCHALLENGE_RENDERERSTATE_H
#define RENDERINGCHALLENGE_RENDERERSTATE_H

#include "glm/glm.hpp"

#define AKEY_EVENT_A 29
#define ASCII_AU 65

#define AKeyEvent(c) (AKEY_EVENT_A + (c - ASCII_AU))

typedef struct InputEventState {

    bool toggleFlag = false;
    bool panFlag = false;

    bool moveTarget = false;

    /*
     * 0b01 -> Move Camera Position
     * 0b10 -> Move Target Position
     * 0b11 -> Move Both Camera and Target Together
     */
    uint8_t moveCode = 1;

    // Stores the pointer position for touch/move events
    glm::vec2 pPos;

} InputEventState;


struct RendStateVars {
    bool hasAspectRatio = false;
    bool cameraMoved = false;
};


#endif //RENDERINGCHALLENGE_RENDERERSTATE_H
