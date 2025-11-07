//
// Created by dunca on 2025-11-06.
//

#ifndef RENDERINGCHALLENGE_INPUTEVENTSTATE_H
#define RENDERINGCHALLENGE_INPUTEVENTSTATE_H

#include "glm/glm.hpp"

#define AKEY_EVENT_A 29
#define ASCII_AU 65

#define AKeyEvent(c) (AKEY_EVENT_A + (c - ASCII_AU))

typedef struct InputEventState {

    bool toggleFlag = false;
    bool panFlag = false;

    // Stores the pointer position for touch/move events
    glm::vec2 pPos;
    int sensitivity = 1;

} InputEventState;


#endif //RENDERINGCHALLENGE_INPUTEVENTSTATE_H
