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

#define LOG_TAG GPUPostProcessor

#include "GPUPostProcessor.h"

#include "iutils/CameraLog.h"

namespace icamera {

GPUPostProcessor::GPUPostProcessor(int cameraId, const stream_t& srcStream,
                                   const stream_t& dstStream)
        : PostProcessorBase("gpu"),
          mCameraId(cameraId),
          mSrcStream(srcStream),
          mDstStream(dstStream) {
    CLEAR(mContext);
    LOG1("@%s, src %dx%d format:%x, dst %dx%d format:%x", __func__, mSrcStream.width,
         mSrcStream.height, mSrcStream.format, mDstStream.width, mDstStream.height,
         mDstStream.format);
}

GPUPostProcessor::~GPUPostProcessor() {
    LOG1(" %s", __func__);
    destroyContext();
}

void GPUPostProcessor::renderBuffers(const void* in, void* out) {
    GLuint attachments[] = {GL_COLOR_ATTACHMENT0};
    glBindFramebuffer(GL_FRAMEBUFFER, mContext.fbo);
    for (int i = PLANE_Y; i < PLANE_MAX; i++) {
        if (i == PLANE_UV)
            glViewport(0, 0, mDstStream.width / 2, mDstStream.height / 2);
        else
            glViewport(0, 0, mDstStream.width, mDstStream.height);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);

        // bind the output target to the frame buffer object
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mContext.outTexture[i], 0);
        GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("Framebuffer object not ready :%x", status);
            break;
        }

        // Use the program object
        glUseProgram(mContext.program[i]);

        // Bind the input texture
        glBindTexture(GL_TEXTURE_2D, mContext.inTexture[i]);
        if (i == PLANE_Y) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mSrcStream.width, mSrcStream.height, GL_RED,
                            GL_UNSIGNED_BYTE, in);
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mSrcStream.width / 2, mSrcStream.height / 2,
                            GL_RG, GL_UNSIGNED_BYTE,
                            (const char*)in + mSrcStream.width * mSrcStream.height);
        }

        glDrawBuffers(1, attachments);
        // sampler index, we have only 1 sampler in each program
        glUniform1i(mContext.sampler[i], 0);
        // Load the vertex position of the target, here is the output rectangle
        glVertexAttribPointer(mContext.vertextLoc[i], 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                              vVertices);
        // input texture coordinates, set the coordinates to implement rotate/crop/scale
        glVertexAttribPointer(mContext.textureLoc[i], 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                              tVertices);

        glEnableVertexAttribArray(mContext.vertextLoc[i]);
        glEnableVertexAttribArray(mContext.textureLoc[i]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFlush();
        glReadBuffer(attachments[0]);
        if (i == PLANE_Y) {
            glReadPixels(0, 0, mDstStream.width, mDstStream.height, GL_RED, GL_UNSIGNED_BYTE, out);
        } else {
            glReadPixels(0, 0, mDstStream.width / 2, mDstStream.height / 2, GL_RG, GL_UNSIGNED_BYTE,
                         (char*)out + mDstStream.width * mDstStream.height);
        }
        glFinish();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

status_t GPUPostProcessor::doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                            std::shared_ptr<CameraBuffer>& outBuf) {
    // context should be created in the same thread running the gles shaders
    status_t ret = createContext();
    if (ret != OK) {
        LOGE(" %s Failed to create context", __func__);
        destroyContext();
        return ret;
    }

    PERF_CAMERA_ATRACE_PARAM1(mName.c_str(), 0);
    LOG2("@%s processor name: %s", __func__, mName.c_str());
    if (isBypassed(inBuf->getSequence())) {
        MEMCPY_S(outBuf->getBufferAddr(), outBuf->getBufferSize(), inBuf->getBufferAddr(),
                 inBuf->getBufferSize());
    } else {
        cropAndRotate(inBuf->getSequence());
        renderBuffers(inBuf->getBufferAddr(), outBuf->getBufferAddr());
    }

    return OK;
}

status_t GPUPostProcessor::createContext() {
    if (mContext.initialized) return OK;

    mContext.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    CheckAndLogError(mContext.eglDisplay == EGL_NO_DISPLAY, NO_INIT, "Failed to get egl display");

    EGLint major, minor;
    GLboolean ret = eglInitialize(mContext.eglDisplay, &major, &minor);
    LOG1(" %s init EGL version: %d.%d", __func__, major, minor);
    CheckAndLogError(ret != EGL_TRUE, NO_INIT, "failed to initialize egl %d", ret);

    const char* extensions = eglQueryString(mContext.eglDisplay, EGL_EXTENSIONS);
    LOG1(" %s extensions: %s", __func__, extensions);

    // opengl es 3.0, RGB888, dpeth 8bit
    EGLint eglConfigAttribs[] = {EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_ES3_BIT,
                                 EGL_SURFACE_TYPE,
                                 EGL_DONT_CARE,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_DEPTH_SIZE,
                                 8,
                                 EGL_NONE};
    EGLint numConfigs;
    EGLConfig eglConfig;
    ret = eglChooseConfig(mContext.eglDisplay, eglConfigAttribs, &eglConfig, 1, &numConfigs);
    CheckAndLogError(ret != EGL_TRUE, NO_INIT, "choose config failed");

    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint attributes[] = {EGL_WIDTH, mSrcStream.width, EGL_HEIGHT, mSrcStream.height, EGL_NONE};
    mContext.eglSurface = eglCreatePbufferSurface(mContext.eglDisplay, eglConfig, attributes);
    CheckAndLogError(mContext.eglSurface == EGL_NO_SURFACE, NO_INIT, "Failed to create surface");

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    mContext.eglCtx =
        eglCreateContext(mContext.eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    CheckAndLogError(mContext.eglCtx == EGL_NO_CONTEXT, NO_INIT, "Failed to create EGL context");

    ret = eglMakeCurrent(mContext.eglDisplay, mContext.eglSurface, mContext.eglSurface,
                         mContext.eglCtx);
    CheckAndLogError(ret != EGL_TRUE, NO_INIT, "Failed to make current");
    ret = createTextures();
    CheckAndLogError(ret != OK, NO_INIT, "Failed to create textures");
    ret = prepareRenderObject();
    CheckAndLogError(ret != OK, NO_INIT, "Failed to prepare render objects");
    mContext.initialized = true;
    return OK;
}

status_t GPUPostProcessor::createTextures() {
    LOG1(" %s", __func__);
    // Y, UV input and Y, UV output
    GLuint textures[4];
    glGenTextures(4, textures);
    mContext.inTexture[PLANE_Y] = textures[0];
    mContext.inTexture[PLANE_UV] = textures[1];
    mContext.outTexture[PLANE_Y] = textures[2];
    mContext.outTexture[PLANE_UV] = textures[3];

    // initialize the texture image buffer
    for (int i = 0; i < 4; i++) {
        stream_t bindingStream = (i <= 1) ? mSrcStream : mDstStream;
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        if (i % PLANE_MAX == PLANE_Y) {
            // Y has only 1 element per pixel, GL_RED means texture only has 1 color element
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bindingStream.width, bindingStream.height, 0,
                         GL_RED, GL_UNSIGNED_BYTE, NULL);
        } else {
            // UV has only 2 elements per pixel, GL_RG means texture has 2 color elements
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, bindingStream.width / 2, bindingStream.height / 2,
                         0, GL_RG, GL_UNSIGNED_BYTE, NULL);
        }

        // Set the filtering mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return OK;
}

void GPUPostProcessor::destroyContext() {
    LOG1(" %s", __func__);
    for (int i = PLANE_Y; i < PLANE_MAX; i++) {
        if (mContext.program[i] > 0) glDeleteProgram(mContext.program[i]);
        if (mContext.inTexture[i] > 0) glDeleteTextures(1, &mContext.inTexture[i]);
        if (mContext.outTexture[i] > 0) glDeleteTextures(1, &mContext.outTexture[i]);
    }

    if (mContext.fbo > 0) glDeleteFramebuffers(1, &mContext.fbo);

    if (mContext.eglDisplay != EGL_NO_DISPLAY)
        eglMakeCurrent(mContext.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (mContext.eglCtx != EGL_NO_CONTEXT) eglDestroyContext(mContext.eglDisplay, mContext.eglCtx);
    if (mContext.eglSurface != EGL_NO_SURFACE)
        eglDestroySurface(mContext.eglDisplay, mContext.eglSurface);
    if (mContext.eglDisplay != EGL_NO_DISPLAY) eglTerminate(mContext.eglDisplay);

    CLEAR(mContext);
}

GLuint GPUPostProcessor::loadShader(GLenum type, const char* shaderSrc) {
    // Create the shader object, shaderId should > 0 if succeed
    GLuint shaderId = glCreateShader(type);
    CheckAndLogError(shaderId <= 0, BAD_VALUE, "Failed to create shader %d", type);
    // Load the shader source
    glShaderSource(shaderId, 1, &shaderSrc, NULL);
    // Compile the shader
    glCompileShader(shaderId);

    // Check the compile status
    GLint compiled;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            if (infoLog) {
                glGetShaderInfoLog(shaderId, infoLen, NULL, infoLog);
                LOGE("Error compiling shader:%s", infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shaderId);
        return BAD_VALUE;
    }

    return shaderId;
}

GLuint GPUPostProcessor::createProgram(const char* vertextSrc, const char* fragmentSrc) {
    // Load the vertex fragment shaders
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertextSrc);
    CheckAndLogError(vertexShader <= 0, vertexShader, "Failed to create shader %d",
                     GL_VERTEX_SHADER);

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fragmentShader <= 0) {
        glDeleteShader(vertexShader);
        LOGE("Failed to create shader %d", GL_FRAGMENT_SHADER);
        return fragmentShader;
    }

    // Create the program object
    GLuint programObject = glCreateProgram();
    if (programObject > 0) {
        // a common program includes 2 shaders, a verterx and a fragment
        glAttachShader(programObject, vertexShader);
        glAttachShader(programObject, fragmentShader);

        glLinkProgram(programObject);
        GLint linked;
        glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint infoLen = 0;
            glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char* infoLog = (char*)malloc(sizeof(char) * infoLen);
                if (infoLog) {
                    glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
                    LOGE("Error linking program: %s", infoLog);
                    free(infoLog);
                }
            }

            glDeleteProgram(programObject);
            programObject = BAD_VALUE;
        }
    }

    // Free up no longer needed shader resources
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);
    return programObject;
}

status_t GPUPostProcessor::prepareRenderObject() {
    LOG1(" %s", __func__);
    glGenFramebuffers(1, &mContext.fbo);
    CheckAndLogError(mContext.fbo <= 0, NO_INIT, "Failed to create Frame buffer object %d",
                     mContext.fbo);

    mContext.program[PLANE_Y] = createProgram(vertextShader, YShader);
    CheckAndLogError(mContext.program[PLANE_Y] < 0, NO_INIT, "Failed to create program %d",
                     PLANE_Y);
    mContext.program[PLANE_UV] = createProgram(vertextShader, UVShader);
    CheckAndLogError(mContext.program[PLANE_UV] < 0, NO_INIT, "Failed to create program %d",
                     PLANE_UV);
    // we defined vars in both shaders with the same name
    for (int i = PLANE_Y; i < PLANE_MAX; i++) {
        mContext.sampler[i] = glGetUniformLocation(mContext.program[i], "textureIn");
        mContext.vertextLoc[i] = glGetAttribLocation(mContext.program[i], "positionIn");
        mContext.textureLoc[i] = glGetAttribLocation(mContext.program[i], "texCoordIn");
    }
    return OK;
}

void GPUPostProcessor::cropAndRotate(int64_t sequence) {
    auto cameraContext = CameraContext::getInstance(mCameraId);
    auto dataContext = cameraContext->getDataContextBySeq(sequence);

    LOG2("%s zoomRatio:%f  Rotate:%d", __func__, dataContext->zoomRegion.ratio,
         dataContext->zoomRegion.rotateMode);
    MEMCPY_S(vVertices, sizeof(vVertices), vVerticesDefault, sizeof(vVerticesDefault));
    MEMCPY_S(tVertices, sizeof(tVertices), tVerticesDefault, sizeof(tVerticesDefault));

    /* operation order:
    ** 1. digital zoom via cropRegion or zoomRatio
    ** 2. rotate to the requested orientation
    ** 3. crop and scale to fit the aspect ratio
    */
    float startX = 0.0f;
    float startY = 0.0f;
    float texWidth = 1.0f;
    float texHeight = 1.0f;
    float srcRatio = (float)mSrcStream.width / mSrcStream.height;

    // when zoom ratio is 1.0, should use crop region to do the digital zoom
    if ((abs(dataContext->zoomRegion.ratio - 1.0f) < 0.001f) &&
        (dataContext->zoomRegion.right * dataContext->zoomRegion.bottom > 0)) {
        startX = (float)dataContext->zoomRegion.left / mSrcStream.width;
        startY = (float)dataContext->zoomRegion.bottom / mSrcStream.height;
        texWidth = (float)(dataContext->zoomRegion.right - dataContext->zoomRegion.left) /
                   mSrcStream.width;
        texHeight = (float)(dataContext->zoomRegion.top - dataContext->zoomRegion.bottom) /
                    mSrcStream.height;
        srcRatio = (float)(dataContext->zoomRegion.right - dataContext->zoomRegion.left) /
                   (dataContext->zoomRegion.top - dataContext->zoomRegion.bottom);
    } else if (dataContext->zoomRegion.ratio > 1.0f + 0.001f) {
        // zoom in, crop the source image
        texWidth = 1.0f / dataContext->zoomRegion.ratio;
        texHeight = texWidth;
        startX = 0.5f - texWidth / 2;
        startY = startX;
    } else if (dataContext->zoomRegion.ratio < 1.0f - 0.001f) {
        // ratio < 1.0f, zoom out, render the source to the region of target
        // x start = -1.0f + (1.0f - dataContext->zoomRegion.ratio);
        float xy = -dataContext->zoomRegion.ratio;
        float wh = dataContext->zoomRegion.ratio * 2.0f;
        // Position 0
        vVertices[0] = xy;
        vVertices[1] = xy;
        // Position 1
        vVertices[3] = xy;
        vVertices[4] = xy + wh;
        // Position 2
        vVertices[6] = xy + wh;
        vVertices[7] = xy + wh;
        // Position 3
        vVertices[9] = xy + wh;
        vVertices[10] = xy;
    }

    float dstRatio = (float)mDstStream.width / mDstStream.height;
    // rotateMode AUTO = 90
    if (dataContext->zoomRegion.rotateMode != icamera::ROTATE_NONE &&
        dataContext->zoomRegion.rotateMode != icamera::ROTATE_180) {
        // the source image width > height, when rotate 90 or 270, need crop the width
        int w = mSrcStream.width * texWidth;
        int h = mSrcStream.height * texHeight;
        startX = startX + (texWidth - (float)h * h / w / w) / 2;
        texWidth = (float)h * h / w / mSrcStream.width;
    } else if (srcRatio > dstRatio + 0.001f) {
        // crop horizon
        startX = (texHeight * dstRatio) / texWidth / 2 + startX;
        texWidth = texHeight * dstRatio;
    } else if (srcRatio + 0.001f < dstRatio) {
        // crop vertical
        startY = (texWidth / dstRatio) / texHeight;
    }

    // crop input texture
    tVertices[0] = startX;
    tVertices[2] = startX;
    tVertices[4] = startX + texWidth;
    tVertices[6] = startX + texWidth;

    tVertices[1] = startY;
    tVertices[3] = startY + texHeight;
    tVertices[5] = startY + texHeight;
    tVertices[7] = startY;

    int rot = dataContext->zoomRegion.rotateMode;
    if (rot > 0) {
        // rotate input texture
        GLfloat tVer[sizeof(tVertices) / sizeof(GLfloat)];
        /* rotate the texture by changing the vertext draw order, the new coordinate index
        ** will be (i + rot) % 4, rotate 180 [0, 1 ,2 ,3] --> [2, 3, 0, 1]
        */
        for (int i = 0; i < sizeof(tVertices) / sizeof(GLfloat); i++) {
            // each vertext has 2 element, float array index should be 2* rot
            tVer[i] = tVertices[(i + 2 * rot) % (sizeof(tVertices) / sizeof(GLfloat))];
        }
        MEMCPY_S(tVertices, sizeof(tVertices), tVer, sizeof(tVer));
    }
}

bool GPUPostProcessor::isBypassed(int64_t sequence) {
    auto cameraContext = CameraContext::getInstance(mCameraId);
    auto dataContext = cameraContext->getDataContextBySeq(sequence);

    int32_t width = dataContext->zoomRegion.right - dataContext->zoomRegion.left;
    int32_t height = dataContext->zoomRegion.top - dataContext->zoomRegion.bottom;
    if (dataContext->zoomRegion.rotateMode == icamera::ROTATE_NONE &&
        (abs(dataContext->zoomRegion.ratio - 1.0f) < 0.001f) &&
        (((width == mSrcStream.width) && (height == mSrcStream.height)) || width == 0 ||
         height == 0))
        return true;
    return false;
}

}  // namespace icamera
