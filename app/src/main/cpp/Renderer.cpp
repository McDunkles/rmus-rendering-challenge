#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <assert.h>

#include "AndroidOut.h"
#include "glm/glm.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "../../../../tools/PointCloudData.h"
#include "Octree.h"

#include <iostream>
#include <fstream>
#include <iomanip>

#include <cstring>

using cpoint_t = struct Point;

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}


// Vertex shader for colored triangle
static const char *vertex = R"vertex(#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

out vec3 fragColor;

uniform mat4 uProjection;
uniform mat4 modelMat;
uniform mat4 viewMat;

void main() {
    fragColor = inColor/255.0f;
    gl_Position = uProjection * modelMat * viewMat * vec4(inPosition, 1.0);
}
)vertex";

// Fragment shader for colored triangle
static const char *fragment = R"fragment(#version 300 es
precision mediump float;

in vec3 fragColor;

out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)fragment";

/*!
 * The near plane distance for the projection matrix. Since this is an orthographic projection
 * matrix, it's convenient to have negative values for sorting (and avoiding z-fighting at 0).
 */
// static constexpr float kProjectionNearPlane = 0.5f;

/*!
 * The far plane distance for the projection matrix. Since this is an orthographic porjection
 * matrix, it's convenient to have the far plane equidistant from 0 as the near plane.
 */
// static constexpr float kProjectionFarPlane = 30.f;

void printMatrix(glm::mat4& matrix, const std::string& name) {

    int matLen = glm::mat4::length();
    aout << "Printing Matrix " << name <<  ": (Length = " << matLen << ") {\n";
    for (int i = 0; i < matLen; i++) {
        for (int j = 0; j<matLen; j++) {
            aout << matrix[j][i] << ", ";
        }
        aout << "\n";
    }
    aout << "}\n";

}


std::string pointAsString(cpoint_t& point) {

    char carr[100];
    std::sprintf(carr, "Point {Loc(%f, %f, %f), Col(%hhu, %hhu, %hhu)}",
                 point.x, point.y, point.z, point.r, point.g, point.b);

    return {carr};
}


void printPointData(cpoint_t *buffer, int size, int start, int end) {

    int numElems = end - start;

    if (numElems > size) {
        aout << "Cannot print " << numElems << " elements from a buffer of size " << size << "...\n";
        return;
    }

    aout << "Printing Point Data " <<  ": (Length = " << numElems << ") {\n";
    for (int i = 0; i < numElems; i++) {
        std::string pointStr = pointAsString(buffer[start + i]);
            aout << "buffer[" << i << "] = " << pointStr << "\n";
    }
    aout << "}\n";

}


Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        // Clean up OpenGL resources
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
        }
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
        }
        if (shader_program_) {
            glDeleteProgram(shader_program_);
        }
        
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}


// Aspect ratio must be known first
void Renderer::setRenderBoxes() {

    // Let's start with four boxes

    float renderDepth = camera_.zFar - camera_.zNear;

    std::vector<float> depths = {
            camera_.zNear,
            camera_.zNear + (renderDepth/3.f),
            camera_.zNear + (2.f*renderDepth/3.f),
            camera_.zFar
    };

    for (int i = 0; i<depths.size(); i++) {
        float vert = (camera_.distScalarY) * depths[i];
        float horiz = vert*camera_.aspectRatio;
        // renderBoxes[i]
    }

    // renderBoxes[0]

}


void Renderer::render() {
    // Check to see if the surface has changed size. This is _necessary_ to do every frame when
    // using immersive mode as you'll get no other notification that your renderable area has
    // changed.
    updateRenderArea();

    bool mySignal = false;

    // When the renderable area changes, the projection matrix has to also be updated. This is true
    // even if you change from the sample orthographic projection matrix as your aspect ratio has
    // likely changed.
    if (shaderNeedsNewProjectionMatrix_) {

        // Create a simple orthographic projection matrix
        float aspectRatio = float(width_) / float(height_);
        camera_.aspectRatio = aspectRatio;


        camera_.calcDistScalar();


        glm::mat4 perspectiveMat = glm::perspective
                (camera_.fovy, aspectRatio,
                 camera_.zNear, camera_.zFar);

        aout << "Camera distScalar = " << camera_.distScalarY << "\n";
        printMatrix(perspectiveMat, "PERSPECTIVE MATRIX");

        glm::mat4 projectionMatrix = perspectiveMat;

        // Set the projection matrix uniform
        glUseProgram(shader_program_);
        GLint projectionLoc = glGetUniformLocation(shader_program_, "uProjection");
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

        // make sure the matrix isn't generated every frame
        shaderNeedsNewProjectionMatrix_ = false;
        mySignal = true;
    }

    if (updateViewMatrix_) {

        // aout << "Updating view matrix!\n";

        // Update view matrix
        this->camera_.updateViewMatrix();
        glm::mat4 viewMatrix = this->camera_.viewMatrix_;
        // printMatrix(viewMatrix, "viewMatrix");

        glUseProgram(shader_program_);
        GLint viewMatLoc = glGetUniformLocation(shader_program_, "viewMat");
        glUniformMatrix4fv(viewMatLoc, 1, GL_FALSE, &(viewMatrix[0][0]));


        updateViewMatrix_ = false;
    }

    // clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the triangle
    glUseProgram(shader_program_);

/*
    // Update view matrix
    glm::mat4 viewMatrix = this->camera_.viewMatrix_;
    // viewMatrix = glm::translate(viewMatrix, {slider_, 0, 0});

    if (mySignal) {
        printMatrix(viewMatrix, "viewMatrix1");
    }

    GLint viewMatLoc = glGetUniformLocation(shader_program_, "viewMat");
    glUniformMatrix4fv(viewMatLoc, 1, GL_FALSE, &(viewMatrix[0][0]));
*/

    glBindVertexArray(vao_);
    // glDrawArrays(GL_POINTS, 0, 10);
    // glDrawArrays(GL_LINES, 0, 10);
    // glDrawArrays(GL_LINE_LOOP, 0, 10);
    glDrawArrays(GL_LINE_STRIP, 0, 5000);
    // glDrawArrays(GL_TRIANGLES, 0, 9);
    glBindVertexArray(0);

    // Present the rendered image. This is an implicit glFlush.
    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

// No this isn't safe, quick work-around to render the colour
void populateDataBuffer(std::vector<cpoint_t>& srcData, float dest[1024][6], int numElems) {

    for (int i = 0; i<numElems; i++) {
        dest[i][0] = srcData[i].x;
        dest[i][1] = srcData[i].y;
        dest[i][2] = srcData[i].z;

        dest[i][3] = ((float)srcData[i].r)/255.0f;
        dest[i][4] = ((float)srcData[i].g)/255.0f;
        dest[i][5] = ((float)srcData[i].b)/255.0f;
    }

}


void Renderer::initCore() {
    // Choose your render attributes
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    // The default display is probably what you want on Android
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    // get the list of configurations
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    aout << "Found config with " << red << ", " << green << ", " << blue << ", "
                         << depth << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    aout << "Found " << numConfigs << " configs" << std::endl;
    aout << "Chose " << config << std::endl;

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    // get some window metrics
    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;


    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    aout << "CORE INIT: (width, height) = (" << width << ", " << height << ")\n";
}

void Renderer::initShaders() {
    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

    // setup any other gl related global states
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // enable alpha globally for now, you probably don't want to do this in a game
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex, nullptr);
    glCompileShader(vertexShader);

    // Check vertex shader compilation
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        aout << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment, nullptr);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        aout << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    // Create shader program and link
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertexShader);
    glAttachShader(shader_program_, fragmentShader);
    glLinkProgram(shader_program_);

    // Check program linking
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(shader_program_, 512, nullptr, infoLog);
        aout << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    // Delete shaders as they're linked into program now
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}


void Renderer::initCamera() {
    // Initialize the Camera
    // glm::vec3 pos = {0.0f, 0.0f, 3.0f};
    // glm::vec3 target = {0.0f, 0.0f, 0.0f};
    glm::vec3 pos = {-5.0f, -20.0f, 0.f};
    glm::vec3 target = {-5.0f, -30.0f, -25.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    this->camera_ = Camera(pos, up);

    if (stateVars.hasAspectRatio) {
        camera_.calcDistScalar();
    }
}

typedef struct ChunkMetaMetaData {



} CMMD_t;


void Renderer::initData() {
    aout << "Attempting to read in point cloud data...\n";

    AAssetManager *assetManager = app_->activity->assetManager;
    AAsset *cloudData = AAssetManager_open(assetManager, "pointcloud_1m.pcd", AASSET_MODE_RANDOM);


    FileHeader header;

    AAsset_read(cloudData,&header, sizeof(FileHeader));

    absoluteBounds = header.bounds;

    int chunk_count = header.chunk_count;
    aout << "Initializing dataset... there are [" << chunk_count << "] chunks in the data\n";

    std::vector<ChunkMetadata> chunkData(chunk_count);
    AAsset_read(cloudData,chunkData.data(), sizeof(ChunkMetadata) * chunk_count);

    aout << "Chunk metadata read in... ready to start loading in point cloud data!\n";

    OctreeNode root(absoluteBounds, 0, 0);

    for (int i = 0; i<chunk_count; i++) {
        root.insert(chunkData[i].bbox);
    }

    // OctreeNode desiredNode = root.getNode(targetPoint)

    // Finds which index the desired node should be at in memory
    // root.getSequentialLoc(desiredNode)

    std::vector<cpoint_t> pc_data(5000);

    // AAsset_seek(cloudData,96, SEEK_SET);
    AAsset_read(cloudData,pc_data.data(), 5000*sizeof(cpoint_t));

    // printPointData(pc_data.data(), 1024, 0, 5);

    for (int i = 0; i<20; i++) {
        pc_data[i].r = 255;
        pc_data[i].g = 0;
        pc_data[i].b = 0;
    }


    aout << "We should now have point cloud data\n";

    // Define triangle vertices with positions (x, y, z) and colors (r, g, b)

    float vertices[] = {
            // Position          // Color (RGB)
            0.0f,  12.0f, 0.0f,  1.0f, 0.0f, 0.0f,  // Top vertex - Red
            -7.0f, -10.0f, 0.0f,  0.0f, 1.0f, 0.0f,  // Bottom left - Green
            8.0f, -15.0f, 0.0f,  0.0f, 0.0f, 1.0f   // Bottom right - Blue
    };


    // Create and bind VAO and VBO
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);


    // 1. Set data to render
    // 2. Position attribute (location 0, first 3 floats)
    // 3. Color attribute (location 1, next 3 floats)

    // For vertices
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices3), vertices3, GL_STATIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    aout << "sizeof(cpoint_t) = " << (sizeof(cpoint_t)) << "\n";
    aout << "pc_data capacity = " << pc_data.capacity() << "\n";

    // For pc_data
    glBufferData(GL_ARRAY_BUFFER, (sizeof(cpoint_t) * pc_data.capacity()), pc_data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(cpoint_t), (void*)0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(cpoint_t), (void*)(3 * sizeof(float)));


    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Unbind VAO
    glBindVertexArray(0);

    aout << "Triangle initialized successfully" << std::endl;
}


void Renderer::initRenderer() {

    aout << "Test... AKey_Event_D = " << AKeyEvent('D') << "\n";

    // 1. Initialize the display, surface, and context objects
    initCore();

    // 2. Initialize the shaders and attach them to the shader program
    initShaders();

    // 3. Initialize model matrix
    glm::mat4 modelMatrix = {1.0f};
    // modelMatrix = glm::rotate(modelMatrix, angle_, {0.0f, 0.0f, 1.0f});

    // Set the model matrix uniform
    glUseProgram(shader_program_);
    GLint modelMatLoc = glGetUniformLocation(shader_program_, "modelMat");
    glUniformMatrix4fv(modelMatLoc, 1, GL_FALSE, &(modelMatrix[0][0]));

    // 4. Initialize our 'Camera', which will contain our view matrix
    initCamera();

    // 6. Okay... let's try loading in vertices
    initData();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    // aout << "INFO :: (width, height) = (" << width << ", " << height << ")\n";

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);

        // make sure that we lazily recreate the projection matrix before we render
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::handleInput() {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        aout << "Pointer(s): ";

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Down";

                inState.pPos = {x, y};
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Up";
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                    aout << "(" << pointer.id << ", " << x << ", " << y << ")";

                    if (index != (motionEvent.pointerCount - 1)) aout << ",";
                    aout << " ";
                }


                if (inState.toggleFlag) {
                    // Update camera position
                    float dx = x - inState.pPos[0];
                    float dy = y - inState.pPos[1];

                    float viewY = camera_.distScalarY * camera_.targetDist;
                    float viewX = viewY * camera_.aspectRatio;

                    // aout << "[RENDERER] (viewX, viewY) = (" << viewX << ", " << viewY << ")\n";
                    // aout << "[RENDERER] (dx, dy) = (" << dx << ", " << dy << ")\n";

                    float diffX = (dx/width_);
                    float diffY = (dy/width_);

                    glm::vec2 tiltVector = {diffX, diffY};

                    // aout << "[RENDERER] (diffX, diffY) = (" << diffX << ", " << diffY << ")\n";

                    // Do the thing
                    camera_.camTilt(tiltVector);
                    updateViewMatrix_ = true;
                }

                inState.pPos = {x, y};

                aout << "Pointer Move";
                break;
            default:
                aout << "Unknown MotionEvent Action: " << action;
        }
        aout << std::endl;
    }
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        aout << "Key: " << keyEvent.keyCode <<" ";
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                aout << "Key Down";
                break;
            case AKEY_EVENT_ACTION_UP:
                aout << "Key Up";

                switch (keyEvent.keyCode) {
                    case AKeyEvent('F'): // 'f' key (x++)
                        camera_.pos_[0]++;
                        aout << "camera.x = " << camera_.pos_[0];
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('S'): // 's' key (x--)
                        camera_.pos_[0]--;
                        aout << "camera.x = " << camera_.pos_[0];
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('E'): // 'e' key (y++)
                        camera_.pos_[1]++;
                        aout << "camera.y = " << camera_.pos_[1];
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('D'): // 'd' key (y--)
                        camera_.pos_[1]--;
                        aout << "camera.y = " << camera_.pos_[1];
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('W'): // 'w' key (z++)
                        camera_.pos_[2]++;
                        aout << "camera.z = " << camera_.pos_[2];
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('Q'): // 'q' key (z--)
                        camera_.pos_[2]--;
                        aout << "camera.z = " << camera_.pos_[2] << "\n";
                        updateViewMatrix_ = true;
                        break;
                    case AKeyEvent('T'):
                        inState.toggleFlag = !inState.toggleFlag;
                        aout << "Toggling Cam Tilt Flag " <<
                        ((inState.toggleFlag)? "ON" : "OFF" ) << "\n";
                        break;
                    case AKeyEvent('P'):
                        inState.panFlag = !inState.panFlag;
                        aout << "Toggling Camera Pan Flag " <<
                        ((inState.panFlag)? "ON" : "OFF" ) << "\n";
                        break;

                }

                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                aout << "Multiple Key Actions";
                break;
            default:
                aout << "Unknown KeyEvent Action: " << keyEvent.action;
        }
        aout << std::endl;
    }
    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}