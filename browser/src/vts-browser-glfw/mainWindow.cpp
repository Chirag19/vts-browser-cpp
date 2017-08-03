/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <limits>
#include <cmath>
#include <cstdio>
#include <unistd.h>

#include <vts-browser/map.hpp>
#include <vts-browser/statistics.hpp>
#include <vts-browser/draws.hpp>
#include <vts-browser/buffer.hpp>
#include <vts-browser/resources.hpp>
#include <vts-browser/options.hpp>
#include <vts-browser/exceptions.hpp>
#include <vts-browser/credits.hpp>
#include <vts-browser/log.hpp>
#include <vts-browser/celestial.hpp>
#include "mainWindow.hpp"
#include <GLFW/glfw3.h>

namespace
{

using vts::readInternalMemoryBuffer;

void mousePositionCallback(GLFWwindow *window, double xpos, double ypos)
{
    MainWindow *m = (MainWindow*)glfwGetWindowUserPointer(window);
    m->gui.mousePositionCallback(xpos, ypos);
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    MainWindow *m = (MainWindow*)glfwGetWindowUserPointer(window);
    m->gui.mouseButtonCallback(button, action, mods);
}

void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    MainWindow *m = (MainWindow*)glfwGetWindowUserPointer(window);
    m->gui.mouseScrollCallback(xoffset, yoffset);
}

void keyboardCallback(GLFWwindow *window, int key, int scancode,
                                            int action, int mods)
{
    MainWindow *m = (MainWindow*)glfwGetWindowUserPointer(window);
    m->gui.keyboardCallback(key, scancode, action, mods);
}

void keyboardUnicodeCallback(GLFWwindow *window, unsigned int codepoint)
{
    MainWindow *m = (MainWindow*)glfwGetWindowUserPointer(window);
    m->gui.keyboardUnicodeCallback(codepoint);
}

double sqr(double a)
{
	return a * a;
}

} // namespace

AppOptions::AppOptions() :
    screenshotOnFullRender(false),
    closeOnFullRender(false),
    renderAtmosphere(true)
{}

MainWindow::MainWindow(vts::Map *map, const AppOptions &appOptions) :
    appOptions(appOptions),
    camNear(0), camFar(0),
    mousePrevX(0), mousePrevY(0),
    dblClickInitTime(0), dblClickState(0),
    width(0), height(0), widthPrev(0), heightPrev(0),
    frameBufferId(0), depthTexId(0), colorTexId(0),
    map(map), window(nullptr)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    glfwWindowHint(GLFW_VISIBLE, true);
    window = glfwCreateWindow(800, 600, "renderer-glfw", NULL, NULL);
    if (!window)
        throw std::runtime_error("Failed to create window "
                                 "(unsupported OpenGL version?)");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    gladLoadGLLoader((GLADloadproc)&glfwGetProcAddress);
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, &::mousePositionCallback);
    glfwSetMouseButtonCallback(window, &::mouseButtonCallback);
    glfwSetScrollCallback(window, &::mouseScrollCallback);
    glfwSetKeyCallback(window, &::keyboardCallback);
    glfwSetCharCallback(window, &::keyboardUnicodeCallback);

    // check for extensions
    anisotropicFilteringAvailable
            = glfwExtensionSupported("GL_EXT_texture_filter_anisotropic");
    openglDebugAvailable
            = glfwExtensionSupported("GL_KHR_debug");

    initializeGpuContext();

    // load shader texture
    {
        shaderTexture = std::make_shared<GpuShaderImpl>();
        vts::Buffer vert = readInternalMemoryBuffer(
                    "data/shaders/texture.vert.glsl");
        vts::Buffer frag = readInternalMemoryBuffer(
                    "data/shaders/texture.frag.glsl");
        shaderTexture->loadShaders(
            std::string(vert.data(), vert.size()),
            std::string(frag.data(), frag.size()));
        std::vector<vts::uint32> &uls = shaderTexture->uniformLocations;
        GLuint id = shaderTexture->id;
        uls.push_back(glGetUniformLocation(id, "uniMvp"));
        uls.push_back(glGetUniformLocation(id, "uniUvMat"));
        uls.push_back(glGetUniformLocation(id, "uniUvMode"));
        uls.push_back(glGetUniformLocation(id, "uniMaskMode"));
        uls.push_back(glGetUniformLocation(id, "uniTexMode"));
        uls.push_back(glGetUniformLocation(id, "uniAlpha"));
        glUseProgram(id);
        glUniform1i(glGetUniformLocation(id, "texColor"), 0);
        glUniform1i(glGetUniformLocation(id, "texMask"), 1);
        glUseProgram(0);
    }

    // load shader color
    {
        shaderColor = std::make_shared<GpuShaderImpl>();
        vts::Buffer vert = readInternalMemoryBuffer(
                    "data/shaders/color.vert.glsl");
        vts::Buffer frag = readInternalMemoryBuffer(
                    "data/shaders/color.frag.glsl");
        shaderColor->loadShaders(
            std::string(vert.data(), vert.size()),
            std::string(frag.data(), frag.size()));
        std::vector<vts::uint32> &uls = shaderColor->uniformLocations;
        GLuint id = shaderColor->id;
        uls.push_back(glGetUniformLocation(id, "uniMvp"));
        uls.push_back(glGetUniformLocation(id, "uniColor"));
    }

    // load shader atmosphere
    {
        shaderAtmosphere = std::make_shared<GpuShaderImpl>();
        vts::Buffer vert = readInternalMemoryBuffer(
                    "data/shaders/atmosphere.vert.glsl");
        vts::Buffer frag = readInternalMemoryBuffer(
                    "data/shaders/atmosphere.frag.glsl");
        shaderAtmosphere->loadShaders(
            std::string(vert.data(), vert.size()),
            std::string(frag.data(), frag.size()));
        std::vector<vts::uint32> &uls = shaderAtmosphere->uniformLocations;
        GLuint id = shaderAtmosphere->id;
        uls.push_back(glGetUniformLocation(id, "uniColorLow"));
        uls.push_back(glGetUniformLocation(id, "uniColorHigh"));
        uls.push_back(glGetUniformLocation(id, "uniRadiuses"));
        uls.push_back(glGetUniformLocation(id, "uniDepths"));
        uls.push_back(glGetUniformLocation(id, "uniFog"));
        uls.push_back(glGetUniformLocation(id, "uniAura"));
        uls.push_back(glGetUniformLocation(id, "uniCameraPosition"));
        uls.push_back(glGetUniformLocation(id, "uniCameraPosNorm"));
        uls.push_back(glGetUniformLocation(id, "uniCameraDirections[0]"));
        uls.push_back(glGetUniformLocation(id, "uniCameraDirections[1]"));
        uls.push_back(glGetUniformLocation(id, "uniCameraDirections[2]"));
        uls.push_back(glGetUniformLocation(id, "uniCameraDirections[3]"));
        glUseProgram(id);
        glUniform1i(glGetUniformLocation(id, "texDepth"), 6);
        glUniform1i(glGetUniformLocation(id, "texColor"), 7);
        glUseProgram(0);
    }

    // load shader blit
    {
        shaderBlit = std::make_shared<GpuShaderImpl>();
        vts::Buffer vert = readInternalMemoryBuffer(
                    "data/shaders/blit.vert.glsl");
        vts::Buffer frag = readInternalMemoryBuffer(
                    "data/shaders/blit.frag.glsl");
        shaderBlit->loadShaders(
            std::string(vert.data(), vert.size()),
            std::string(frag.data(), frag.size()));
        //std::vector<vts::uint32> &uls = shaderBlit->uniformLocations;
        GLuint id = shaderBlit->id;
        glUseProgram(id);
        glUniform1i(glGetUniformLocation(id, "texColor"), 7);
        glUseProgram(0);
    }

    // load mesh mark
    {
        meshMark = std::make_shared<GpuMeshImpl>();
        vts::GpuMeshSpec spec(vts::readInternalMemoryBuffer(
                                  "data/meshes/sphere.obj"));
        assert(spec.faceMode == vts::GpuMeshSpec::FaceMode::Triangles);
        spec.attributes.resize(1);
        spec.attributes[0].enable = true;
        spec.attributes[0].stride = sizeof(vts::vec3f) + sizeof(vts::vec2f);
        spec.attributes[0].components = 3;
        vts::ResourceInfo info;
        meshMark->loadMesh(info, spec);
    }

    // load mesh line
    {
        meshLine = std::make_shared<GpuMeshImpl>();
        vts::GpuMeshSpec spec(vts::readInternalMemoryBuffer(
                                  "data/meshes/line.obj"));
        assert(spec.faceMode == vts::GpuMeshSpec::FaceMode::Lines);
        spec.attributes.resize(1);
        spec.attributes[0].enable = true;
        spec.attributes[0].stride = sizeof(vts::vec3f) + sizeof(vts::vec2f);
        spec.attributes[0].components = 3;
        vts::ResourceInfo info;
        meshLine->loadMesh(info, spec);
    }

    // load mesh quad
    {
        meshQuad = std::make_shared<GpuMeshImpl>();
        vts::GpuMeshSpec spec(vts::readInternalMemoryBuffer(
                                  "data/meshes/quad.obj"));
        assert(spec.faceMode == vts::GpuMeshSpec::FaceMode::Triangles);
        spec.attributes.resize(2);
        spec.attributes[0].enable = true;
        spec.attributes[0].stride = sizeof(vts::vec3f) + sizeof(vts::vec2f);
        spec.attributes[0].components = 3;
        spec.attributes[1].enable = true;
        spec.attributes[1].stride = sizeof(vts::vec3f) + sizeof(vts::vec2f);
        spec.attributes[1].components = 2;
        spec.attributes[1].offset = sizeof(vts::vec3f);
        vts::ResourceInfo info;
        meshQuad->loadMesh(info, spec);
    }
}

MainWindow::~MainWindow()
{
    if (map)
        map->renderFinalize();
    glfwDestroyWindow(window);
    window = nullptr;
}

void MainWindow::mousePositionCallback(double xpos, double ypos)
{
    double p[3] = { xpos - mousePrevX, ypos - mousePrevY, 0 };
    int mode = 0;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
            mode = 2;
        else
            mode = 1;
    }
    else
    {
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
        || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
            mode = 2;
    }
    switch (mode)
    {
    case 1:
        map->pan(p);
        break;
    case 2:
        map->rotate(p);
        break;
    }
    mousePrevX = xpos;
    mousePrevY = ypos;
}

void MainWindow::mouseButtonCallback(int button, int action, int mods)
{
    static const double dblClickThreshold = 0.22;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        double n = glfwGetTime();
        switch (action)
        {
        case GLFW_PRESS:
            if (dblClickState == 2 && dblClickInitTime + dblClickThreshold > n)
            {
                mouseDblClickCallback(mods);
                dblClickState = 0;
            }
            else
            {
                dblClickInitTime = n;
                dblClickState = 1;
            }
            break;
        case GLFW_RELEASE:
            if (dblClickState == 1 && dblClickInitTime + dblClickThreshold > n)
                dblClickState = 2;
            else
                dblClickState = 0;
            break;
        }
    }
    else
    {
        dblClickInitTime = 0;
        dblClickState = 0;
    }
}

void MainWindow::mouseDblClickCallback(int)
{
    vts::vec3 posPhys = getWorldPositionFromCursor();
    if (posPhys(0) != posPhys(0))
        return;
    double posNav[3];
    map->convert(posPhys.data(), posNav,
                 vts::Srs::Physical, vts::Srs::Navigation);
    map->setPositionPoint(posNav, vts::NavigationType::Quick);
}

void MainWindow::mouseScrollCallback(double, double yoffset)
{
    map->zoom(yoffset * 120);
}

void MainWindow::keyboardCallback(int key, int, int action, int)
{
    // marks
    if (action == GLFW_RELEASE && key == GLFW_KEY_M)
    {
        Mark mark;
        mark.coord = getWorldPositionFromCursor();
        if (mark.coord(0) != mark.coord(0))
            return;
        marks.push_back(mark);
        colorizeMarks();
    }
}

void MainWindow::keyboardUnicodeCallback(unsigned int)
{
    // do nothing
}

void MainWindow::drawVtsTask(const vts::DrawTask &t)
{
    if (t.texColor)
    {
        shaderTexture->bind();
        shaderTexture->uniformMat4(0, t.mvp);
        shaderTexture->uniformMat3(1, t.uvm);
        shaderTexture->uniform(2, (int)t.externalUv);
        if (t.texMask)
        {
            shaderTexture->uniform(3, 1);
            glActiveTexture(GL_TEXTURE0 + 1);
            ((GpuTextureImpl*)t.texMask.get())->bind();
            glActiveTexture(GL_TEXTURE0 + 0);
        }
        else
            shaderTexture->uniform(3, 0);
        GpuTextureImpl *tex = (GpuTextureImpl*)t.texColor.get();
        tex->bind();
        shaderTexture->uniform(4, (int)tex->grayscale);
        shaderTexture->uniform(5, t.color[3]);
    }
    else
    {
        shaderColor->bind();
        shaderColor->uniformMat4(0, t.mvp);
        shaderColor->uniformVec4(1, t.color);
    }
    GpuMeshImpl *m = (GpuMeshImpl*)t.mesh.get();
    m->bind();
    m->dispatch();
}

void MainWindow::drawMark(const Mark &m, const Mark *prev)
{
    vts::mat4 mvp = camViewProj
            * vts::translationMatrix(m.coord)
            * vts::scaleMatrix(map->getPositionViewExtent() * 0.005);
    vts::mat4f mvpf = mvp.cast<float>();
    vts::DrawTask t;
    vts::vec4f c = vts::vec3to4f(m.color, 1);
    for (int i = 0; i < 4; i++)
        t.color[i] = c(i);
    t.mesh = meshMark;
    memcpy(t.mvp, mvpf.data(), sizeof(t.mvp));
    drawVtsTask(t);
    if (prev)
    {
        t.mesh = meshLine;
        mvp = camViewProj * vts::lookAt(m.coord, prev->coord);
        mvpf = mvp.cast<float>();
        memcpy(t.mvp, mvpf.data(), sizeof(t.mvp));
        drawVtsTask(t);
    }
}

void MainWindow::renderFrame()
{
    checkGl("pre-frame check");

    // update framebuffer texture
    {
        if (width != widthPrev || height != heightPrev)
        {
            widthPrev = width;
            heightPrev = height;

            // depth texture
            glActiveTexture(GL_TEXTURE0 + 6);
            glDeleteTextures(1, &depthTexId);
            glGenTextures(1, &depthTexId);
            glBindTexture(GL_TEXTURE_2D, depthTexId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height,
                         0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            // color texture
            glActiveTexture(GL_TEXTURE0 + 7);
            glDeleteTextures(1, &colorTexId);
            glGenTextures(1, &colorTexId);
            glBindTexture(GL_TEXTURE_2D, colorTexId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height,
                         0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            glActiveTexture(GL_TEXTURE0);

            // frame buffer
            glDeleteFramebuffers(1, &frameBufferId);
            glGenFramebuffers(1, &frameBufferId);
            glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 depthTexId, 0);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 colorTexId, 0);

            checkGl("update frame buffer");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
        checkGlFramebuffer();
    }

    // initialize opengl
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    checkGl("frame initialized");

    // vts draws
    {
        const vts::MapDraws &draws = map->draws();
        for (const vts::DrawTask &t : draws.draws)
            drawVtsTask(t);
        glBindVertexArray(0);
    }

    // marks draws
    {
        shaderColor->bind();
        meshMark->bind();
        Mark *prevMark = nullptr;
        for (Mark &mark : marks)
        {
            drawMark(mark, prevMark);
            prevMark = &mark;
        }
        glBindVertexArray(0);
    }

    checkGl("frame content rendered");

    // render the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    shaderBlit->bind();
    meshQuad->bind();
    meshQuad->dispatch();

    // render atmosphere
    const vts::MapCelestialBody &body = map->celestialBody();
    if (appOptions.renderAtmosphere
            && body.majorRadius > 0 && body.atmosphereThickness > 0)
    {
        glEnable(GL_BLEND);

        vts::mat4 inv = camViewProj.inverse();
        vts::vec3 camPos = vts::vec4to3(inv * vts::vec4(0, 0, -1, 1), true);
        double camRad = vts::length(camPos);
        double atmRad = body.majorRadius + body.atmosphereThickness;
        double aurDotLow = camRad > body.majorRadius
                ? -sqrt(sqr(camRad) - sqr(body.majorRadius)) / camRad : 0;
        double aurDotHigh = camRad > atmRad
                ? -sqrt(sqr(camRad) - sqr(atmRad)) / camRad : 0;
        aurDotHigh = std::max(aurDotHigh, aurDotLow + 1e-4);

        map->statistics().debug = aurDotLow;

        vts::vec3f uniCameraPosition = camPos.cast<float>();
        vts::vec3f uniCameraPosNorm = vts::normalize(camPos).cast<float>();
        float uniRadiuses[4]
            = { (float)body.majorRadius, (float)body.minorRadius,
                (float)body.atmosphereThickness };
        float uniDepths[4] = { (float)camNear, (float)camFar };
        float uniFog[4] = { 0.f, 50000.f }; // todo fog distance relative to body.majorRadius
        float uniAura[4] = { (float)aurDotLow, (float)aurDotHigh };

        vts::vec3 near = vts::vec4to3(inv * vts::vec4(0, 0, -1, 1), true);
        vts::vec3f uniCameraDirections[4] = {
            vts::normalize(vts::vec4to3(inv * vts::vec4(-1, -1, 1, 1)
                , true)- near).cast<float>(),
            vts::normalize(vts::vec4to3(inv * vts::vec4(+1, -1, 1, 1)
                , true)- near).cast<float>(),
            vts::normalize(vts::vec4to3(inv * vts::vec4(-1, +1, 1, 1)
                , true)- near).cast<float>(),
            vts::normalize(vts::vec4to3(inv * vts::vec4(+1, +1, 1, 1)
                , true)- near).cast<float>(),
        };

        shaderAtmosphere->bind();
        shaderAtmosphere->uniformVec4(0, body.atmosphereColorLow);
        shaderAtmosphere->uniformVec4(1, body.atmosphereColorHigh);
        shaderAtmosphere->uniformVec4(2, uniRadiuses);
        shaderAtmosphere->uniformVec4(3, uniDepths);
        shaderAtmosphere->uniformVec4(4, uniFog);
        shaderAtmosphere->uniformVec4(5, uniAura);
        shaderAtmosphere->uniformVec3(6, (float*)uniCameraPosition.data());
        shaderAtmosphere->uniformVec3(7, (float*)uniCameraPosNorm.data());
        for (int i = 0; i < 4; i++)
        {
            shaderAtmosphere->uniformVec3(8 + i,
                            (float*)uniCameraDirections[i].data());
        }

        meshQuad->bind();
        meshQuad->dispatch();
    }

    checkGl("frame finalized");
}

void MainWindow::loadTexture(vts::ResourceInfo &info,
                             const vts::GpuTextureSpec &spec)
{
    auto r = std::make_shared<GpuTextureImpl>();
    r->loadTexture(info, spec);
    info.userData = r;
}

void MainWindow::loadMesh(vts::ResourceInfo &info,
                          const vts::GpuMeshSpec &spec)
{
    auto r = std::make_shared<GpuMeshImpl>();
    r->loadMesh(info, spec);
    info.userData = r;
}

void MainWindow::run()
{
    glfwGetFramebufferSize(window, &width, &height);
    map->setWindowSize(width, height);

    // this application uses separate thread for resource processing,
    //   therefore it is safe to process as many resources as possible
    //   in single dataTick without causing any lag spikes
    map->options().maxResourceProcessesPerTick = -1;

    setMapConfigPath(appOptions.paths[0]);
    map->callbacks().loadTexture = std::bind(&MainWindow::loadTexture, this,
                std::placeholders::_1, std::placeholders::_2);
    map->callbacks().loadMesh = std::bind(&MainWindow::loadMesh, this,
                std::placeholders::_1, std::placeholders::_2);
    map->callbacks().cameraOverrideView
            = std::bind(&MainWindow::cameraOverrideView,
                                this, std::placeholders::_1);
    map->callbacks().cameraOverrideProj
            = std::bind(&MainWindow::cameraOverrideProj,
                                this, std::placeholders::_1);
    map->callbacks().cameraOverrideFovAspectNearFar = std::bind(
                &MainWindow::cameraOverrideParam, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4);
    map->renderInitialize();
    gui.initialize(this);

    bool initialPositionSet = false;

    while (!glfwWindowShouldClose(window))
    {
        if (!initialPositionSet && map->isMapConfigReady())
        {
            initialPositionSet = true;
            if (!appOptions.initialPosition.empty())
            {
                try
                {
                    map->setPositionUrl(appOptions.initialPosition,
                                        vts::NavigationType::Instant);
                }
                catch (...)
                {
                    vts::log(vts::LogLevel::warn3,
                             "failed to set initial position");
                }
            }
        }

        checkGl("frame begin");
        double timeFrameStart = glfwGetTime();

        try
        {
            glfwGetFramebufferSize(window, &width, &height);
            map->setWindowSize(width, height);
            map->renderTickPrepare();
            map->renderTickRender(); // calls camera overrides
            camViewProj = camProj * camView;
        }
        catch (const vts::MapConfigException &e)
        {
            std::stringstream s;
            s << "Exception <" << e.what() << ">";
            vts::log(vts::LogLevel::err4, s.str());
            if (appOptions.paths.size() > 1)
                setMapConfigPath(MapPaths());
            else
                throw;
        }
        double timeMapRender = glfwGetTime();

        renderFrame();

        double timeAppRender = glfwGetTime();

        gui.input(); // calls glfwPollEvents()
        gui.render(width, height);
        double timeGui = glfwGetTime();

        if (map->statistics().renderTicks % 120 == 0)
        {
            std::string creditLine = std::string() + "vts-browser-glfw: "
                    + map->credits().textFull();
            glfwSetWindowTitle(window, creditLine.c_str());
        }

        glfwSwapBuffers(window);
        double timeFrameFinish = glfwGetTime();

        // temporary workaround for when v-sync is missing
        long duration = (timeFrameFinish - timeFrameStart) * 1000000;
        if (duration < 16000)
        {
            usleep(16000 - duration);
            timeFrameFinish = glfwGetTime();
        }

        timingMapProcess = timeMapRender - timeFrameStart;
        timingAppProcess = timeAppRender - timeMapRender;
        timingGuiProcess = timeGui - timeAppRender;
        timingTotalFrame = timeFrameFinish - timeFrameStart;

        if (appOptions.closeOnFullRender && map->isMapRenderComplete())
            glfwSetWindowShouldClose(window, true);
    }
    gui.finalize();
}

void MainWindow::colorizeMarks()
{
    if (marks.empty())
        return;
    float mul = 1.0f / marks.size();
    int index = 0;
    for (Mark &m : marks)
        m.color = vts::convertHsvToRgb(vts::vec3f(index++ * mul, 1, 1));
}

vts::vec3 MainWindow::getWorldPositionFromCursor()
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    y = height - y - 1;
    float depth = std::numeric_limits<float>::quiet_NaN();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBufferId);
    glReadPixels((int)x, (int)y, 1, 1,
                 GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    if (depth > 1 - 1e-7)
        depth = std::numeric_limits<float>::quiet_NaN();
    depth = depth * 2 - 1;
    x = x / width * 2 - 1;
    y = y / height * 2 - 1;
    return vts::vec4to3(camViewProj.inverse()
                              * vts::vec4(x, y, depth, 1), true);
}

void MainWindow::cameraOverrideParam(double &, double &,
                                     double &near, double &far)
{
    camNear = near;
    camFar = far;
}

void MainWindow::cameraOverrideView(double *mat)
{
    for (int i = 0; i < 16; i++)
        camView(i) = mat[i];
}

void MainWindow::cameraOverrideProj(double *mat)
{
    for (int i = 0; i < 16; i++)
        camProj(i) = mat[i];
}

void MainWindow::setMapConfigPath(const MapPaths &paths)
{
    map->setMapConfigPath(paths.mapConfig, paths.auth, paths.sri);
}

