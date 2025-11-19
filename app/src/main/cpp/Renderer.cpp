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
    gl_Position = uProjection * viewMat * modelMat * vec4(inPosition, 1.0);
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

    if (pcd_file.is_open()) {
        pcd_file.close();
    }

    if (pcd_file_dbg.is_open()) {
        pcd_file_dbg.close();
    }

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
void Renderer::setRenderBox() {

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

        if (camera_.pos_ == camera_.target_) {

            aout << "(pos and target can't be equal)\n";

            switch (inState.lastKeyPressed) {
                case AKeyEvent('S'): // 's' key (x--)
                    if (inState.moveCode & 0b1) {
                        camera_.target_[0]--;
                        aout << "target.x = " << camera_.target_[0] << "\n";
                    }

                    if (inState.moveCode & 0b10) {
                        camera_.pos_[0]--;
                        aout << "camera.x = " << camera_.pos_[0] << "\n";
                    }

                    aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                         ", " << camera_.pos_.z << ")\n";
                    aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                         ", " << camera_.target_.z << ")\n";

                    updateViewMatrix_ = true;
                    stateVars.cameraMoved = true;
                    break;
                case AKeyEvent('E'): // 'e' key (y++)
                    if (inState.moveCode & 0b1) {
                        camera_.target_[1]++;
                        aout << "target.y = " << camera_.target_[1] << "\n";
                    }

                    if (inState.moveCode & 0b10) {
                        camera_.pos_[1]++;
                        aout << "camera.y = " << camera_.pos_[1] << "\n";
                    }

                    aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                         ", " << camera_.pos_.z << ")\n";
                    aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                         ", " << camera_.target_.z << ")\n";

                    updateViewMatrix_ = true;
                    stateVars.cameraMoved = true;
                    break;
                case AKeyEvent('D'): // 'd' key (y--)

                    if (inState.moveCode & 0b1) {
                        camera_.target_[1]--;
                        aout << "target.y = " << camera_.target_[1] << "\n";
                    }

                    if (inState.moveCode & 0b10) {
                        camera_.pos_[1]--;
                        aout << "camera.y = " << camera_.pos_[1] << "\n";
                    }

                    aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                         ", " << camera_.pos_.z << ")\n";
                    aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                         ", " << camera_.target_.z << ")\n";

                    updateViewMatrix_ = true;
                    stateVars.cameraMoved = true;
                    break;

                case AKeyEvent('W'): // 'w' key (z++)

                    if (inState.moveCode & 0b1) {
                        camera_.target_[2]++;
                        aout << "target.z = " << camera_.target_[2] << "\n";
                    }

                    if (inState.moveCode & 0b10) {
                        camera_.pos_[2]++;
                        aout << "camera.z = " << camera_.pos_[2] << "\n";
                    }

                    aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                         ", " << camera_.pos_.z << ")\n";
                    aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                         ", " << camera_.target_.z << ")\n";

                    updateViewMatrix_ = true;
                    stateVars.cameraMoved = true;
                    break;
                case AKeyEvent('Q'): // 'q' key (z--)

                    if (inState.moveCode & 0b1) {
                        camera_.target_[2]--;
                        aout << "target.z = " << camera_.target_[2] << "\n";
                    }

                    if (inState.moveCode & 0b10) {
                        camera_.pos_[2]--;
                        aout << "camera.z = " << camera_.pos_[2] << "\n";
                    }

                    aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                         ", " << camera_.pos_.z << ")\n";
                    aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                         ", " << camera_.target_.z << ")\n";

                    updateViewMatrix_ = true;
                    stateVars.cameraMoved = true;
                    break;
            }
        }

        // Update view matrix
        this->camera_.updateViewMatrix();
        glm::mat4 viewMatrix = this->camera_.viewMatrix_;
        // printMatrix(viewMatrix, "viewMatrix");

        glUseProgram(shader_program_);
        GLint viewMatLoc = glGetUniformLocation(shader_program_, "viewMat");
        glUniformMatrix4fv(viewMatLoc, 1, GL_FALSE, &(viewMatrix[0][0]));

        glm::mat4 vM = camera_.viewMatrix_;

        glm::vec4 vX = vM[0];
        glm::vec4 vY = vM[1];
        glm::vec4 vZ = vM[2];
        glm::vec4 vO = vM[3];

        // float alpha2 = acos(glm::dot(vX, {1, 0, 0, 0}));
        // float beta2 = acos(glm::dot(vY, {0, 1, 0, 0}));
        // float gamma2 = acos(glm::dot(vZ, {0, 0, 1, 0}));

        float vXx2 = pow(vX[0], 2);
        float vXy2 = pow(vX[1], 2);

        float beta = atan2(-vX[2], sqrt(vXx2 + vXy2));
        float alpha = atan2(vY[2]/cos(beta), vZ[2]/cos(beta));
        float gamma = atan2(vX[1]/cos(beta), vX[0]/cos(beta));

        aout << "vX = (" << vX[0] << ", " << vX[1] << ", " << vX[2] << ", " << vX[3] << ")\n";
        aout << "vY = (" << vY[0] << ", " << vY[1] << ", " << vY[2] << ", " << vY[3] << ")\n";
        aout << "vZ = (" << vZ[0] << ", " << vZ[1] << ", " << vZ[2] << ", " << vZ[3] << ")\n";
        aout << "Euler Angles = (" << alpha << ", " << beta << ", " << gamma << ")\n";


        updateViewMatrix_ = false;
    }

    // Update the rendered chunks if necessary
    if (stateVars.cameraMoved) {
        updateChunks();
        stateVars.cameraMoved = false;
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

    for (int i = 0; i<renderBox.totalSize; i++) {
        if (renderBox.active_indices[i]) {
            glDrawArrays(GL_LINE_STRIP, renderBox.chunk_size * i,
                         (renderBox.chunk_size * i) + renderBox.num_points_array[i]);
        }
    }

    // glDrawArrays(GL_LINE_STRIP, 0, 10000);
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

    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

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

    updateRenderArea();

    /*
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);
*/

    if (height_ != 0 && width_ != 0) {
        aout << "Has aspect ratio\n";
        stateVars.hasAspectRatio = true;
    }


    aout << "CORE INIT: (width, height) = (" << width_ << ", " << height_ << ")\n";
}

void Renderer::initShaders() {

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    // PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);


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



/*
 * dim:
 * 0 -> x
 * 1 -> y
 * 2 -> z
 *
 * returns the incrmented posCode
 */
uint32_t shiftPosCode(uint32_t dim, uint32_t posCodeOld, int maxDepth, bool backwards = false) {

    uint32_t bitMaskXYZ = 0;

    for (int i = 0; i<maxDepth; i++) {
        bitMaskXYZ |= (1) << (3*i + dim);
    }

    uint32_t posCodeMasked = posCodeOld & bitMaskXYZ;
    uint32_t posCodeNew = posCodeOld;

    uint32_t bitMaskTemp = 0b111;
    uint32_t slidingBitMask = 0;

    std::vector<bool> posCodeBits(maxDepth, false);
    bool stacked = false;

    for (int i = 0; i<maxDepth; i++) {

        slidingBitMask = (1 << (3*i));

        posCodeBits[i] = (posCodeMasked >> (3*i + dim)) & bitMaskTemp;

        if (backwards) {
            stacked = !(std::any_of(posCodeBits.begin(), posCodeBits.begin()+i, [](bool n) {return n;}) );
        } else {
            stacked = std::all_of(posCodeBits.begin(), posCodeBits.begin()+i, [](bool n) {return n;});
        }
        posCodeNew = posCodeNew & (~(slidingBitMask << dim));
        posCodeNew |= (stacked ^ (uint32_t)posCodeBits[i]) << (3*i + dim);
    }

    // aout << "Incremented posCodeBL_Y2 = " << posCodeBL_Y2 << "\n";

    return posCodeNew;
}


uint32_t shiftPosCodeMulti(glm::vec<3, int, glm::defaultp> dim, uint32_t posCodeOld, int maxDepth) {

    uint32_t posCodeNew = posCodeOld;

    int temp = 0;
    bool backwards = false;

    for (int i = 0; i<dim.length(); i++) {
        temp = abs(dim[i]);
        backwards = (dim[i] < 0);

        for (int j = 0; j<temp; j++) {
            posCodeNew = shiftPosCode(i, posCodeNew, maxDepth, backwards);
        }

    }

    return posCodeNew;
}


glm::vec<3, uint32_t, glm::defaultp> Renderer::getIndices(uint32_t posCode) {

    // Maybe for now, just use the far plane as the box bounds

    OctreeNode *root = octreeData.root;
    int maxDepth = octreeData.maxDepth;

    // get x index
    uint32_t bitMaskX = 0;
    uint32_t bitMaskY = 0;
    uint32_t bitMaskZ = 0;

    for (int i = 0; i<maxDepth; i++) {
        bitMaskX |= (1) << (3*i);
        bitMaskY |= (1) << (3*i + 1);
        bitMaskZ |= (1) << (3*i + 2);
    }

    // aout << "bitMaskX = " << bitMaskX << "\n";
    // aout << "bitMaskY = " << bitMaskY << "\n";
    // aout << "bitMaskZ = " << bitMaskZ << "\n";


    uint32_t posCodeX = posCode & bitMaskX;
    uint32_t posCodeY = posCode & bitMaskY;
    uint32_t posCodeZ = posCode & bitMaskZ;

    // aout << "posCodeTR_Z = " << posCodeTR_Z << "\n";

    uint32_t indexX = 0;
    uint32_t indexY = 0;
    uint32_t indexZ = 0;

    uint32_t bitMaskTemp = 0b111;
    uint32_t temp = 0;

    for (int i = 0; i<maxDepth; i++) {
        // Bottom Left indices
        temp = (posCodeX >> (3*i)) & bitMaskTemp;
        indexX |= (temp << i);

        temp = (posCodeY >> (3*i)) & bitMaskTemp;
        temp = temp >> 1;
        indexY |= (temp << i);

        temp = (posCodeZ >> (3*i)) & bitMaskTemp;
        temp = temp >> 2;
        indexZ |= (temp << i);
    }

    // aout << "[getIndices] posCode = " << posCode << "; Indicies:\n x = " << indexX <<
    // "; y = " << indexY << "; z = " << indexZ << "\n";

    glm::vec<3, uint32_t, glm::defaultp> indices = {indexX, indexY, indexZ};

    return indices;
}

glm::vec3 Renderer::getIndicesFloat(glm::vec3 point) {

    BoundingBox aB = octreeData.absoluteBounds;
    glm::vec3 minPoint = {aB.min_x, aB.min_y, aB.min_z};
    glm::vec3 unitBox = octreeData.unitBoxDims;

    glm::vec3 indices = {
            (point.x - minPoint.x)/unitBox.x,
            (point.y - minPoint.y)/unitBox.y,
            (point.z - minPoint.z)/unitBox.z
    };

    return indices;
}


void Renderer::fetchChunks2() {

    OctreeNode *root = octreeData.root;
    int maxDepth = octreeData.maxDepth;

    float halfHeight = camera_.zFar * camera_.distScalarY;
    float halfWidth = halfHeight * camera_.aspectRatio;

    // Let's get the 8 corners
    // 0 BLN, 1 BRN, 2 TLN, 3 TRN, 4 BLF, 5 BRF, 6 TLF, 7 TRF
    std::vector<glm::vec3> corners(8);
    for (int i = 0; i<8; i++) {
        corners[i].x = camera_.pos_.x + halfWidth*pow(-1, i);
        corners[i].y = camera_.pos_.y + halfHeight*(((i % 4) < 2)? -1 : 1);
        corners[i].z = camera_.pos_.z - ((i < 4)? camera_.zFar : 0);
    }

    /*
    glm::vec3 botLeftNear = {
            camera_.pos_.x - halfWidth,
            camera_.pos_.y - halfHeight,
            camera_.pos_.z - camera_.zFar
    };

    glm::vec3 botRightNear = {
            camera_.pos_.x + halfWidth,
            camera_.pos_.y - halfHeight,
            camera_.pos_.z - camera_.zFar
    };

    glm::vec3 topLeftNear = {
            camera_.pos_.x - halfWidth,
            camera_.pos_.y + halfHeight,
            camera_.pos_.z - camera_.zFar
    };

    glm::vec3 topRightNear = {
            camera_.pos_.x + halfWidth,
            camera_.pos_.y + halfHeight,
            camera_.pos_.z - camera_.zFar
    };


    glm::vec3 botLeftFar = {
            camera_.pos_.x - halfWidth,
            camera_.pos_.y - halfHeight,
            camera_.pos_.z
    };

    glm::vec3 botRightFar = {
            camera_.pos_.x + halfWidth,
            camera_.pos_.y - halfHeight,
            camera_.pos_.z
    };

    glm::vec3 topLeftFar = {
            camera_.pos_.x - halfWidth,
            camera_.pos_.y + halfHeight,
            camera_.pos_.z
    };

    glm::vec3 topRightFar = {
            camera_.pos_.x + halfWidth,
            camera_.pos_.y + halfHeight,
            camera_.pos_.z
    };
    */


    /*
    // These are described in local space, which must be converted to camera
    // space before performing any operations
    glm::vec3 sideX = botRightNear - botLeftNear;
    glm::vec3 sideY = topLeftNear - botLeftNear;
    glm::vec3 sideZ = botLeftFar - botRightFar;

    glm::mat4 vM = glm::transpose(camera_.viewMatrix_);


    aout << "Bottom Left Point: (x, y, z) = (" << botLeftNear.x << ", " <<
         botLeftNear.y << ", " << botLeftNear.z << ")\n";

    uint32_t posCodeBL = root->getPosCode(botLeftNear, maxDepth);
    aout << "posCodeBL = " << posCodeBL << "\n";
    */
}

void Renderer::fetchChunks() {
    // Assume renderBoxes is already filled

    // Maybe for now, just use the far plane as the box bounds

    OctreeNode *root = octreeData.root;
    int maxDepth = octreeData.maxDepth;

    float halfHeight = camera_.zFar * camera_.distScalarY;
    float halfWidth = halfHeight * camera_.aspectRatio;


    /*
    glm::vec3 botLeftPos = {
            camera_.pos_.x - halfWidth,
            camera_.pos_.y - halfHeight,
            camera_.pos_.z - camera_.zFar
    };


    glm::vec3 topRightPos = {
            camera_.pos_.x + halfWidth,
            camera_.pos_.y + halfHeight,
            camera_.pos_.z
    };
    */

    glm::vec3 botLeftPos = {
            camera_.pos_.x - renderBox.cubeSideLength/2,
            camera_.pos_.y - renderBox.cubeSideLength/2,
            camera_.pos_.z - renderBox.cubeSideLength
    };

    glm::vec3 topRightPos = {
            camera_.pos_.x + renderBox.cubeSideLength/2,
            camera_.pos_.y + renderBox.cubeSideLength/2,
            camera_.pos_.z
    };

    // If the camera is pointing toward the positive x axis, move the render box to the
    // other side of the z axis relative to the camera
    if (camera_.pos_.z < camera_.target_.z) {
        botLeftPos.z += renderBox.cubeSideLength;
        topRightPos.z += renderBox.cubeSideLength;
    }


    aout << "Bottom Left Point: (x, y, z) = (" << botLeftPos.x << ", " <<
         botLeftPos.y << ", " << botLeftPos.z << ")\n";

    uint32_t posCodeBL = root->getPosCode(botLeftPos, maxDepth);
    aout << "posCodeBL = " << posCodeBL << "\n";


    uint32_t posCodeTR = root->getPosCode(topRightPos, maxDepth);
    // aout << "Top Right Point: (x, y, z) = (" << topRightPos.x << ", " <<
    //      topRightPos.y << ", " << topRightPos.z << ")\n";
    // aout << "posCodeTR = " << posCodeTR << "\n";

    glm::vec<3, uint32_t, glm::defaultp> indicesBL = getIndices(posCodeBL);
    glm::vec<3, uint32_t, glm::defaultp> indicesTR = getIndices(posCodeTR);

    // aout << "Bottom Left RenderBox Indicies:\n x = " << indexBL_X <<
    // "; y = " << indexBL_Y << "; z = " << indexBL_Z << "\n";

    aout << "Top Right RenderBox Indicies:\n x = " << indicesTR.x <<
         "; y = " << indicesTR.y << "; z = " << indicesTR.z << "\n";

    // Record indices in the corners, which will be used to for comparison
    // when updating chunks
    renderBox.posCodeBL = posCodeBL;
    renderBox.posCodeTR = posCodeTR;
    renderBox.indicesBL = {indicesBL.x, indicesBL.y, indicesBL.z};
    renderBox.indicesTR = {indicesTR.x, indicesTR.y, indicesTR.z};

    uint32_t startingIndex = posCodeBL;

    OctreeNode *nodeBL = root->getNodeSoft(posCodeBL, maxDepth);

    // uint32_t bitMaskX
    int lenX = indicesTR.x - indicesBL.x;
    int lenY = indicesTR.y - indicesBL.y;
    int lenZ = indicesTR.z - indicesBL.z;

    uint32_t posCodeTemp = posCodeBL;
    int rb_index = 0;

    int totalSpan = (lenX+1)*(lenY+1)*(lenZ+1);

    if (totalSpan > renderBox.totalSize) {
        aout << "Problem: Total Size = " << totalSpan << ", but renderBox.totalSize = "
        << renderBox.totalSize << "\n";

    } else {
        aout << "Loading in Chunks: Total Size = " << totalSpan
        << "; renderBox.totalSize = "
         << renderBox.totalSize << "\n";

        // Do the loading stuff

        OctreeNode *currNode = nullptr, *prevNode = nullptr;

        int nodes_loaded = 0, nodes_bounced = 0;

        glm::vec<3, uint32_t, glm::defaultp> currIndices = {0, 0, 0};
        int iX = 0, iY = 0, iZ = 0;
        int count = 0;

        for (int i = 0; i <= lenZ; i++) {
            for (int j = 0; j <= lenY; j++) {
                for (int k = 0; k <= lenX; k ++) {

                    currNode = root->getNodeSoft(posCodeTemp, maxDepth);
                    currIndices = getIndices(posCodeTemp);

                    iX = currIndices.x % renderBox.bufferDims.x;
                    iY = currIndices.y % renderBox.bufferDims.y;
                    iZ = currIndices.z % renderBox.bufferDims.z;

                    rb_index = iX + (renderBox.bufferDims.x * iY)
                               + (renderBox.bufferDims.y * renderBox.bufferDims.x * iZ);

                    if (currNode != nullptr) {

                        if (prevNode == nullptr ||
                        (currNode->encodedPosition != prevNode->encodedPosition)) {

                            /*
                            if (count >= 0) {
                                aout << "rb_index = " << rb_index <<
                                "; posCodeTemp = " << posCodeTemp <<
                                     "; currNode->encPos = " << currNode->encodedPosition << "\n";
                            }
                            */

                            int num_points = currNode->numPoints;

                            // Load the chunk in
                            char *buffer_loc = reinterpret_cast<char*>(renderBox.pcd_buffer.data()) +
                                    (renderBox.chunk_size * rb_index);

                            pcd_file.seekg(currNode->byteOffset, std::ios_base::beg);
                            pcd_file.read(reinterpret_cast<char*>(buffer_loc),
                                            num_points*sizeof(cpoint_t));

                            renderBox.active_indices[rb_index] = true;
                            renderBox.num_points_array[rb_index] = num_points;

                            nodes_loaded++;

                        }

                    }

                    // Update prevNode
                    prevNode = root->getNodeSoft(posCodeTemp, maxDepth);

                    posCodeTemp = shiftPosCode(0, posCodeTemp, maxDepth);

                    count++;
                }

                // Shift posCode back to start in the x direction
                // Yes this kills me to do but... it's simple and will work
                for (int k = 0; k <= lenX; k ++) {
                    posCodeTemp = shiftPosCode(0, posCodeTemp, maxDepth, true);
                }

                posCodeTemp = shiftPosCode(1, posCodeTemp, maxDepth);
            }

            // Shift posCode back to start in the x direction
            // Yes this kills me to do but... it's simple and will work
            for (int j = 0; j <= lenY; j++) {
                posCodeTemp = shiftPosCode(1, posCodeTemp, maxDepth, true);
            }
            posCodeTemp = shiftPosCode(2, posCodeTemp, maxDepth);
        }

        nodes_bounced = totalSpan - nodes_loaded;

        aout << "[fetchChunks] Loaded in " << nodes_loaded << " chunks; Bounced " <<
        nodes_bounced << " chunks.\n";
    }

}


void Renderer::updateChunks() {
    OctreeNode *root = octreeData.root;
    int maxDepth = octreeData.maxDepth;

    float halfHeight = camera_.zFar * camera_.distScalarY;
    float halfWidth = halfHeight * camera_.aspectRatio;


    glm::vec3 botLeftPos = {
            camera_.pos_.x - renderBox.cubeSideLength/2,
            camera_.pos_.y - renderBox.cubeSideLength/2,
            camera_.pos_.z - renderBox.cubeSideLength
    };

    glm::vec3 topRightPos = {
            camera_.pos_.x + renderBox.cubeSideLength/2,
            camera_.pos_.y + renderBox.cubeSideLength/2,
            camera_.pos_.z
    };

    uint32_t posCodeBL = root->getPosCode(botLeftPos, maxDepth);
    uint32_t posCodeTR = root->getPosCode(topRightPos, maxDepth);

    glm::vec<3, uint32_t, glm::defaultp> indicesBL = getIndices(posCodeBL);
    glm::vec<3, uint32_t, glm::defaultp> indicesTR = getIndices(posCodeTR);


    uint32_t startingIndex = posCodeBL;

    OctreeNode *nodeBL = root->getNodeSoft(posCodeBL, maxDepth);

    // uint32_t bitMaskX
    int lenX = indicesTR.x - indicesBL.x;
    int lenY = indicesTR.y - indicesBL.y;
    int lenZ = indicesTR.z - indicesBL.z;

    uint32_t posCodeTemp = posCodeBL;
    int rb_index = 0;

    int totalSpan = (lenX+1)*(lenY+1)*(lenZ+1);

    if (totalSpan > renderBox.totalSize) {
        aout << "Problem: Total Size = " << totalSpan << ", but renderBox.totalSize = "
             << renderBox.totalSize << "\n";

    } else if ( (renderBox.posCodeBL != posCodeBL) || (renderBox.posCodeTR != posCodeTR) ) {

        // Do the loading stuff

        // Find which chunks are now out of scope, remove those
        // Find which chunks are now in scope, load those in

        OctreeNode *currNode = nullptr, *prevNode = nullptr;

        int nodes_loaded = 0, nodes_bounced = 0;

        glm::vec<3, uint32_t, glm::defaultp> currIndices = {0, 0, 0};
        int iX = 0, iY = 0, iZ = 0;
        int count = 0;

        for (int i = 0; i <= lenZ; i++) {
            for (int j = 0; j <= lenY; j++) {
                for (int k = 0; k <= lenX; k ++) {

                    currNode = root->getNodeSoft(posCodeTemp, maxDepth);
                    currIndices = getIndices(posCodeTemp);

                    iX = currIndices.x % renderBox.bufferDims.x;
                    iY = currIndices.y % renderBox.bufferDims.y;
                    iZ = currIndices.z % renderBox.bufferDims.z;

                    rb_index = iX + (renderBox.bufferDims.x * iY)
                               + (renderBox.bufferDims.y * renderBox.bufferDims.x * iZ);

                    // currIndex falls outside of the previous renderBox bounds -> load the chunk in!
                    if ( ( (renderBox.indicesBL.x > currIndices.x) || (renderBox.indicesTR.x < currIndices.x) ) ||
                        ( (renderBox.indicesBL.y > currIndices.y) || (renderBox.indicesTR.y < currIndices.y) ) ||
                        ( (renderBox.indicesBL.z > currIndices.z) || (renderBox.indicesTR.z < currIndices.z) )
                    ) {

                    aout << "[updateChunks] Indices = (" <<
                    currIndices.x << ", " << currIndices.y << ", " <<
                    currIndices.z << ")\n";

                        if (currNode != nullptr) {

                            if (prevNode == nullptr ||
                                (currNode->encodedPosition != prevNode->encodedPosition)) {

                                if (count >= 0) {
                                    aout << "[uC] rb_index = " << rb_index <<
                                         "; posCodeTemp = " << posCodeTemp <<
                                         "; currNode->encPos = " << currNode->encodedPosition << "\n";
                                }

                                int num_points = currNode->numPoints;

                                // Load the chunk in
                                char *buffer_loc = reinterpret_cast<char*>(renderBox.pcd_buffer.data()) +
                                                   (renderBox.chunk_size * rb_index);

                                pcd_file.seekg(currNode->byteOffset, std::ios_base::beg);
                                pcd_file.read(reinterpret_cast<char*>(buffer_loc),
                                              num_points*sizeof(cpoint_t));

                                renderBox.active_indices[rb_index] = true;
                                renderBox.num_points_array[rb_index] = num_points;

                                nodes_loaded++;

                            }

                        }

                    }

                    // Update prevNode
                    prevNode = root->getNodeSoft(posCodeTemp, maxDepth);

                    posCodeTemp = shiftPosCode(0, posCodeTemp, maxDepth);

                    count++;
                }

                // Shift posCode back to start in the x direction
                // Yes this kills me to do but... it's simple and will work
                for (int k = 0; k <= lenX; k ++) {
                    posCodeTemp = shiftPosCode(0, posCodeTemp, maxDepth, true);
                }

                posCodeTemp = shiftPosCode(1, posCodeTemp, maxDepth);
            }

            // Shift posCode back to start in the x direction
            // Yes this kills me to do but... it's simple and will work
            for (int j = 0; j <= lenY; j++) {
                posCodeTemp = shiftPosCode(1, posCodeTemp, maxDepth, true);
            }
            posCodeTemp = shiftPosCode(2, posCodeTemp, maxDepth);
        }

        nodes_bounced = totalSpan - nodes_loaded;

        aout << "[updateChunks] Loaded in " << nodes_loaded << " chunks; Bounced " <<
             nodes_bounced << " chunks.\n";

        // Update renderBox corner indices
        renderBox.posCodeBL = posCodeBL;
        renderBox.posCodeTR = posCodeTR;
        renderBox.indicesBL = {indicesBL.x, indicesBL.y, indicesBL.z};
        renderBox.indicesTR = {indicesTR.x, indicesTR.y, indicesTR.z};
    } else {
        aout << "[uC] No changed needed.\n";
    }
}


void Renderer::initRenderBox() {

    float sY = camera_.distScalarY;
    float sX = sY * camera_.aspectRatio;

    uint32_t max_cap = RenderBox::MAX_CAPACITY;

    float cx = (sX/octreeData.unitBoxDims.x);
    float cy = (sY/octreeData.unitBoxDims.y);
    float cz = (1/octreeData.unitBoxDims.z);

    float camZ = 1.f;

    float calc_cap =
            (ceil(cx*camZ)+1)*(ceil(cy*camZ)+1)*(ceil(cz*camZ)+1);

    bool done = false;

    while (max_cap > calc_cap) {
        camZ *= 2;

        calc_cap =
                (ceil(cx*camZ)+1)*(ceil(cy*camZ)+1)*(ceil(cz*camZ)+1);
    }

    aout << "Bloated camZ = " << camZ << "\n";

    // Now let's find a happy medium
    float step = camZ/4;

    while (max_cap < calc_cap) {
        camZ -= step;
        step /=2;

        calc_cap =
                (ceil(cx*camZ)+1)*(ceil(cy*camZ)+1)*(ceil(cz*camZ)+1);
    }

    aout << "Calculated camZ = " << camZ << "\n";

    camera_.zFar = camZ;

    // Update camera target
    camera_.target_.z = camera_.pos_.z - (camZ/2);
    camera_.updateViewMatrix();

    float camY = camZ * camera_.distScalarY;
    float camX = camY * camera_.aspectRatio;

    float CTC_len = sqrt(pow(camX, 2) + pow(camY, 2) + pow(camZ, 2));

    aout << "camX = " << camX << "; camY = " << camY << "; camZ = " << camZ << "\n";
    // aout << "ratio_x2 = " << ratio_x2 << "; ratio_y2 = " << ratio_y2 << "; ratio_z2 = " << ratio_z2 << "\n";
    // aout << "Total Cube Size = " << totalCubeSize << "\n";

    renderBox.setDims(camX, camY, camZ, octreeData.unitBoxDims);
}


void Renderer::initCamera() {
    // Initialize the Camera
    glm::vec3 pos = {-15.0f, -15.0f, 8.5f};
    // glm::vec3 target = {0.0f, 0.0f, 0.0f};
    // glm::vec3 pos = {-5.0f, -20.0f, 0.f};
    // glm::vec3 target = {-5.0f, -30.0f, -25.0f};
    glm::vec3 up = {0.0f, 1.0f, 0.0f};
    this->camera_ = Camera(pos, up);

    if (stateVars.hasAspectRatio) {

        float ar = float(width_) / float(height_);
        camera_.setAspectRatio(ar);
        camera_.calcDistScalar();

        aout << "[initCamera] aspectRatio = " << camera_.aspectRatio
             << "; distScalar = " << camera_.distScalarY << "\n";
    }
}


void Renderer::initData() {
    aout << "Attempting to read in point cloud data...\n";

    // 1) Open pcd file
    std::string internal_path(app_->activity->internalDataPath);
    internal_path.append("/pointcloud_10m.pcd");
    aout << "Internal Data Path = " << internal_path << "\n";
    pcd_file.open(internal_path,std::ios::binary | std::ios::in);

    // 2) Read in file header and chunk metadata
    FileHeader header;
    pcd_file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
    octreeData.absoluteBounds = {
            header.bounds.min_x, header.bounds.min_y, header.bounds.min_z,
            header.bounds.max_x, header.bounds.max_y, header.bounds.max_z
    };

    int chunk_count = header.chunk_count;
    aout << "Initializing dataset... there are [" << chunk_count << "] chunks in the data\n";

    std::vector<ChunkMetadata> chunkData(chunk_count);
    pcd_file.read(reinterpret_cast<char*>(chunkData.data()), sizeof(ChunkMetadata) * chunk_count);
    aout << "Chunk metadata read in... ready to start loading in point cloud data!\n";

    // 3. Build out the Octree structure from the header and chunk metadata
    OctreeNode *root = new OctreeNode(octreeData.absoluteBounds, 0, 0);
    octreeData.root = root;

    for (int i = 0; i<chunk_count; i++) {
        root->insert(chunkData[i].bbox, octreeData.absoluteBounds);
    }

    // 4. Auxilliary data (maxDepth, unitBox, posCodes, num_points, byte_offset)
    int maxDepth = root->getMaxDepth(root);
    aout << "[INIT DATA] maxDepth = " << maxDepth << "\n";

    auto numSlices = (float)exp2(maxDepth);
    glm::vec3 unitBox = {1,1,1};
    unitBox.x = (octreeData.absoluteBounds.max_x - octreeData.absoluteBounds.min_x) / numSlices;
    unitBox.y = (octreeData.absoluteBounds.max_y - octreeData.absoluteBounds.min_y) / numSlices;
    unitBox.z = (octreeData.absoluteBounds.max_z - octreeData.absoluteBounds.min_z) / numSlices;

    octreeData.maxDepth = maxDepth;
    octreeData.unitBoxDims = unitBox;

    // Now that the chunks are inserted, let's go back and insert some memory info into them
    root->assignAuxInfo(root, maxDepth);
    root->assignChunkMetadata(chunkData, maxDepth);

    aout << "Num Slices = " << numSlices << "\n";


    aout << "Unit Box Dimensions:\n";
    aout << "(x, y, z) = (" << unitBox.x << ", " <<
         unitBox.y << ", " << unitBox.z << ")\n";


    // Make sure we have a proper aspect ratio by this point !
    // Should be taken care of in initCore()
    initRenderBox();
    renderBox.initBuffer(header.chunk_size);

    // Fetch chunks
    fetchChunks();

    // 5) Okay, now we get posCode from the target point, and load in
    // the corresponding node

    aout << "Camera Target: (x, y, z) = (" << camera_.target_.x << ", " <<
         camera_.target_.y << ", " << camera_.target_.z << ")\n";

    // glm::vec3 target = {-5.0f, -30.0f, -25.0f};
    uint32_t posCode = root->getPosCode(camera_.target_, maxDepth);
    aout << "posCode = " << posCode << "\n";

    OctreeNode *desiredNode = root->getNode(posCode, maxDepth);
    BoundingBox bbox0 = desiredNode->bbox;


    // Camera Position = (-5, -20, 0)
    // Camera Target = (-5, -20, -20)
    /* Desired Node Box
     * x -> (-28.30, -2.26)
     * y -> (-17.89, -13.28)
     * z -> (-24.16, 1.61)
    */

    aout << "[initData] node0->BoundingBox: x = ("
         << bbox0.min_x << ", " << bbox0.max_x
         << "); y = (" << bbox0.min_y << ", " << bbox0.max_y << ")" <<
         "; z = (" << bbox0.min_z << ", " << bbox0.max_z << ")\n";

    // Load in this node
    aout << "Sooooo this OctreeNode here.... octreeNum = " << desiredNode->octantNum
    << "; depth = " << desiredNode->depth << "\n";

    aout << "OctreeNum Lineage: " << desiredNode->getLineageStr() << "\n";

    desiredNode->printNode();


    aout << "We should now have point cloud data\n";

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

    glBufferData(GL_ARRAY_BUFFER, (sizeof(cpoint_t) * renderBox.pcd_buffer.capacity()), renderBox.pcd_buffer.data(), GL_DYNAMIC_DRAW);
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

                /*
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
                */

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
                aout << "Key Down\n";
                break;
            case AKEY_EVENT_ACTION_UP:
                aout << "Key Up\n";

                switch (keyEvent.keyCode) {
                    case AKeyEvent('F'): // 'f' key (x++)

                        if (inState.moveCode & 0b1) {
                            camera_.target_[0]++;
                            aout << "target.x = " << camera_.target_[0] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[0]++;
                            aout << "camera.x = " << camera_.pos_[0] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                           ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('S'): // 's' key (x--)
                        if (inState.moveCode & 0b1) {
                            camera_.target_[0]--;
                            aout << "target.x = " << camera_.target_[0] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[0]--;
                            aout << "camera.x = " << camera_.pos_[0] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                             ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('E'): // 'e' key (y++)
                        if (inState.moveCode & 0b1) {
                            camera_.target_[1]++;
                            aout << "target.y = " << camera_.target_[1] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[1]++;
                            aout << "camera.y = " << camera_.pos_[1] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                             ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('D'): // 'd' key (y--)

                        if (inState.moveCode & 0b1) {
                            camera_.target_[1]--;
                            aout << "target.y = " << camera_.target_[1] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[1]--;
                            aout << "camera.y = " << camera_.pos_[1] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                             ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('W'): // 'w' key (z++)

                        if (inState.moveCode & 0b1) {
                            camera_.target_[2]++;
                            aout << "target.z = " << camera_.target_[2] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[2]++;
                            aout << "camera.z = " << camera_.pos_[2] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                             ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('Q'): // 'q' key (z--)

                        if (inState.moveCode & 0b1) {
                            camera_.target_[2]--;
                            aout << "target.z = " << camera_.target_[2] << "\n";
                        }

                        if (inState.moveCode & 0b10) {
                            camera_.pos_[2]--;
                            aout << "camera.z = " << camera_.pos_[2] << "\n";
                        }

                        aout << "Pos = (" << camera_.pos_.x << ", " << camera_.pos_.y <<
                             ", " << camera_.pos_.z << ")\n";
                        aout << "Target = (" << camera_.target_.x << ", " << camera_.target_.y <<
                             ", " << camera_.target_.z << ")\n";

                        updateViewMatrix_ = true;
                        stateVars.cameraMoved = true;
                        break;
                    case AKeyEvent('T'):
                        inState.moveTarget = !inState.moveTarget;
                        inState.moveCode = (inState.moveCode % 0b11)+0b1;
                        aout << "moveCode = " << (int)inState.moveCode << "\n";
                        break;
                    case AKeyEvent('P'):
                        inState.panFlag = !inState.panFlag;
                        aout << "Toggling Camera Pan Flag " <<
                        ((inState.panFlag)? "ON" : "OFF" ) << "\n";
                        break;

                }

                inState.lastKeyPressed = keyEvent.keyCode;

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