/*
 * Copyright (C) 2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include "PostProcessorBase.h"

namespace icamera {

class GPUPostProcessor : public PostProcessorBase {
 public:
    GPUPostProcessor(int cameraId, const stream_t& srcStream, const stream_t& dstStream);
    virtual ~GPUPostProcessor();
    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);
    virtual bool isBypassed(int64_t sequence);

 private:
    const char* vertextShader =
        "#version 300 es \n"
        "in vec4 positionIn; \n"
        "in vec2 texCoordIn; \n"
        "out vec2 texCoordOut; \n"
        "void main() \n"
        "{ \n"
        "   gl_Position = positionIn; \n"
        "   texCoordOut.x = texCoordIn.x; \n"
        "   texCoordOut.y = texCoordIn.y; \n"
        "} \n";

    const char* YShader =
        "#version 300 es \n"
        "precision mediump float; \n"
        "in vec2 texCoordOut; \n"
        "uniform sampler2D textureIn; \n"
        "layout(location = 0) out float YColor; \n"
        "void main() \n"
        "{ \n"
        "   YColor = texture2D(textureIn, texCoordOut).r; \n"
        "} \n";

    const char* UVShader =
        "#version 300 es \n"
        "precision mediump float; \n"
        "in vec2 texCoordOut; \n"
        "uniform sampler2D textureIn; \n"
        "layout(location = 0) out vec2 UVColor; \n"
        "void main() \n"
        "{ \n"
        "   UVColor.r = texture2D(textureIn, texCoordOut).r; \n"
        "   UVColor.g = texture2D(textureIn, texCoordOut).g; \n"
        "} \n";

 private:
    /* target image rectangle vertext
     *          _______________________(1.0,1.0)
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     * (-1.0,-1.0)______________________|
     */
    GLfloat vVertices[12];
    GLfloat vVerticesDefault[12] = {
        -1.0f, -1.0f, 0.0f,  // Position 0
        -1.0f, 1.0f,  0.0f,  // Position 1
        1.0f,  1.0f,  0.0f,  // Position 2
        1.0f,  -1.0f, 0.0f,  // Position 3
    };

    /* source texture vertext
     *          _______________________(1.0,1.0)
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     *         |                        |
     * (0.0,0.0)________________________|
     */

    GLfloat tVertices[8];
    GLfloat tVerticesDefault[8] = {
        0.0f, 0.0f,  // TexCoord 0
        0.0f, 1.0f,  // TexCoord 1
        1.0f, 1.0f,  // TexCoord 2
        1.0f, 0.0f   // TexCoord 3
    };
    GLushort indices[6] = {0, 3, 2, 0, 2, 1};

 private:
    enum { PLANE_Y = 0, PLANE_UV = 1, PLANE_MAX };

    struct Context {
        EGLDisplay eglDisplay;
        EGLContext eglCtx;
        EGLSurface eglSurface;
        bool initialized;

        // Handle to a program object
        GLuint program[PLANE_MAX];
        // Attribute locations in vertext shader
        GLint vertextLoc[PLANE_MAX];
        GLint textureLoc[PLANE_MAX];
        // Sampler locations in fragment shader
        GLint sampler[PLANE_MAX];

        // frame buffer object, container of the render target
        GLuint fbo;
        GLuint inTexture[PLANE_MAX];
        GLuint outTexture[PLANE_MAX];
    };

    int mCameraId;
    Context mContext;
    stream_t mSrcStream;
    stream_t mDstStream;

 private:
    status_t createContext();
    void destroyContext();
    status_t createTextures();
    /**
     * create gles program
     *
     * \param vertextSrc: vertext shader source code
     * \param fragmentSrc: fragment shader source code
     * \return gles program id.
     */
    GLuint createProgram(const char* vertextSrc, const char* fragmentSrc);
    /**
     * create shader
     *
     * \param type: shader type, vertext or fragment shader
     * \param shaderSrc: shader source code
     * \return shader id when success or <= 0 when failed.
     */
    GLuint loadShader(GLenum type, const char* shaderSrc);
    /**
     * prepare other gles render objects like fbo
     *
     * \return OK when success or < 0 when failed.
     */
    status_t prepareRenderObject();
    void cropAndRotate(int64_t sequence);
    void renderBuffers(const void* in, void* out);

    DISALLOW_COPY_AND_ASSIGN(GPUPostProcessor);
};

}  // namespace icamera
