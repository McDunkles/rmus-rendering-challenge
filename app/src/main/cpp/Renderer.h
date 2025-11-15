#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>

#include <fstream>

#include "Model.h"
#include "Shader.h"

#include "Camera.h"

#include "RendererState.h"
#include "OctreeData.h"


#include "../../../../tools/PointCloudData.h"
#include "RenderBox.h"

struct android_app;

class Renderer {
public:
    /*!
     * @param pApp the android_app this Renderer belongs to, needed to configure GL
     */
    inline explicit Renderer(android_app *pApp) :
            app_(pApp),
            display_(EGL_NO_DISPLAY),
            surface_(EGL_NO_SURFACE),
            context_(EGL_NO_CONTEXT),
            width_(0),
            height_(0),
            shaderNeedsNewProjectionMatrix_(true),
            updateViewMatrix_(true),
            shader_program_(0),
            vao_(0),
            vbo_(0),
            camera_(),
            inState() {
        initRenderer();
    }

    virtual ~Renderer();

    /*!
     * Handles input from the android_app.
     *
     * Note: this will clear the input queue
     */
    void handleInput();

    /*!
     * Renders all the models in the renderer
     */
    void render();

private:

    void initCore();

    void initShaders();

    void initCamera();

    void initData();

    /*!
     * Performs necessary OpenGL initialization. Customize this if you want to change your EGL
     * context or application-wide settings.
     */
    void initRenderer();

    /*!
     * @brief we have to check every frame to see if the framebuffer has changed in size. If it has,
     * update the viewport accordingly
     */
    void updateRenderArea();


    void setRenderBox();

    void initRenderBox();

    void fetchChunks();

    glm::vec3 getIndices(uint32_t posCode);


    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;
    Camera camera_;


    bool shaderNeedsNewProjectionMatrix_;
    bool updateViewMatrix_;
    InputEventState inState;
    RendStateVars stateVars;
    
    // Example: Simple triangle rendering
    GLuint shader_program_;
    GLuint vao_;
    GLuint vbo_;

    BoundingBox absoluteBounds;
    std::vector<glm::vec2> renderBoxes;
    OctreeData octreeData;

    std::vector<std::vector<struct Point>> pc_buffer;
    std::ifstream pcd_file;
    std::ifstream pcd_file_dbg;

    RenderBox renderBox;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H