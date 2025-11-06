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
 * Half the height of the projection matrix. This gives you a renderable area of height 40 ranging
 * from -20 to 20
 */
static constexpr float kProjectionHalfHeight = 20.f;

/*!
 * The near plane distance for the projection matrix. Since this is an orthographic projection
 * matrix, it's convenient to have negative values for sorting (and avoiding z-fighting at 0).
 */
static constexpr float kProjectionNearPlane = -40.f;

/*!
 * The far plane distance for the projection matrix. Since this is an orthographic porjection
 * matrix, it's convenient to have the far plane equidistant from 0 as the near plane.
 */
static constexpr float kProjectionFarPlane = 20.f;

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
        float halfHeight = kProjectionHalfHeight;
        float halfWidth = halfHeight * aspectRatio;
        
        glm::mat4 projectionMatrix = glm::ortho(
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            kProjectionNearPlane, kProjectionFarPlane
        );

        glm::perspective()


        printMatrix(projectionMatrix, "projectionMatrix");

        // Set the projection matrix uniform
        glUseProgram(shader_program_);
        GLint projectionLoc = glGetUniformLocation(shader_program_, "uProjection");
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

        // make sure the matrix isn't generated every frame
        shaderNeedsNewProjectionMatrix_ = false;
        mySignal = true;
    }

    // clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw the triangle
    glUseProgram(shader_program_);


    // Update view matrix
    glm::mat4 viewMatrix = this->camera_.viewMatrix_;
    // viewMatrix = glm::translate(viewMatrix, {slider_, 0, 0});

    if (mySignal) {
        printMatrix(viewMatrix, "viewMatrix1");
    }

    GLint viewMatLoc = glGetUniformLocation(shader_program_, "viewMat");
    glUniformMatrix4fv(viewMatLoc, 1, GL_FALSE, &(viewMatrix[0][0]));


    // glm::translate()

    glBindVertexArray(vao_);
    // glDrawArrays(GL_POINTS, 0, 10);
    // glDrawArrays(GL_LINES, 0, 10);
    // glDrawArrays(GL_LINE_LOOP, 0, 10);
    glDrawArrays(GL_LINE_STRIP, 0, 40);
    // glDrawArrays(GL_TRIANGLES, 0, 3);
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
    glm::vec3 pos = {-40.0f, 0.0f, -40.0f};
    glm::vec3 target = {-40.0f, 0.0f, -50.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    this->camera_ = Camera(pos, target, up);
    this->camera_.initCamera();
}


void Renderer::initData() {
    aout << "Attempting to read in point cloud data...\n";

    AAssetManager *assetManager = app_->activity->assetManager;
    AAsset *cloudData = AAssetManager_open(assetManager, "pointcloud_100k.pcd", AASSET_MODE_BUFFER);

    // cpoint_t pc_data[1024];
    std::vector<cpoint_t> pc_data(1024);
    float pc_data_ext[1024][6];

    AAsset_seek(cloudData,96, SEEK_SET);
    AAsset_read(cloudData,pc_data.data(), 42*sizeof(cpoint_t));

    // printPointData(pc_data.data(), 1024, 0, 5);

    for (int i = 0; i<20; i++) {
        pc_data[i].r = 255;
        pc_data[i].g = 0;
        pc_data[i].b = 0;
    }



    //float (*)[6]
    populateDataBuffer(pc_data, pc_data_ext, 1024);

    aout << "We should now have point cloud data\n";

    // Define triangle vertices with positions (x, y, z) and colors (r, g, b)
    /*
    float vertices[] = {
            // Position          // Color (RGB)
            0.0f,  12.0f, -22.0f,  1.0f, 0.0f, 0.0f,  // Top vertex - Red
            -7.0f, -10.0f, -22.0f,  0.0f, 1.0f, 0.0f,  // Bottom left - Green
            8.0f, -15.0f, -22.0f,  0.0f, 0.0f, 1.0f   // Bottom right - Blue
    };
    */

    float vertices[] = {
            // Position          // Color (RGB)
            -50.0f,  12.0f, -42.0f,  1.0f, 0.0f, 0.0f,  // Top vertex - Red
            -57.0f, -10.0f, -42.0f,  0.0f, 1.0f, 0.0f,  // Bottom left - Green
            -42.0f, -15.0f, -42.0f,  0.0f, 0.0f, 1.0f   // Bottom right - Blue
    };

    cpoint_t vertices2[] = {
            {.x = -45.0f, .y = 12.0f, .z = -42.0f, .r = (uint8_t)255, .g = (uint8_t)0, .b = (uint8_t)0, .padding = 0},
            {.x = -52.0f, .y = -10.0f, .z = -42.0f, .r = (uint8_t)0, .g = (uint8_t)255, .b = (uint8_t)0, .padding = 0},
            {.x = -37.0f, .y = -15.0f, .z = -42.0f, .r = (uint8_t)0, .g = (uint8_t)0, .b = (uint8_t)255, .padding = 0}
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
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    aout << "sizeof(cpoint_t) = " << (sizeof(cpoint_t)) << "\n";
    aout << "pc_data capacity = " << pc_data.capacity() << "\n";

    // For pc_data
    glBufferData(GL_ARRAY_BUFFER, (sizeof(cpoint_t) * pc_data.capacity()), pc_data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(cpoint_t), (void*)0);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(cpoint_t), (void*)(3 * sizeof(float)));


    // For vertices2
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2), vertices2, GL_STATIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(cpoint_t), (void*)0);
    // glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(cpoint_t), (void*)(3 * sizeof(float)));

    // For pc_data_ext
    // glBufferData(GL_ARRAY_BUFFER, sizeof(pc_data_ext), pc_data_ext, GL_STATIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Unbind VAO
    glBindVertexArray(0);

    aout << "Triangle initialized successfully" << std::endl;
}


void Renderer::initRenderer() {

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

    // 5. Initialize view matrix
    glm::mat4 viewMatrix = this->camera_.viewMatrix_;
    // printMatrix(viewMatrix, "viewMatrix");

    GLint viewMatLoc = glGetUniformLocation(shader_program_, "viewMat");
    glUniformMatrix4fv(viewMatLoc, 1, GL_FALSE, &(viewMatrix[0][0]));


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