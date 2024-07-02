/*
 * Copyright (C) 2024 Intel Corporation.
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

#define LOG_TAG MockCameraHal

#include "MockCameraHal.h"

#include <libcamera/control_ids.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <memory>

#include "CameraLog.h"
#include "Errors.h"
#include "FileSource.h"
#include "PlatformData.h"
#include "iutils/CameraDump.h"
#include "libcamera/internal/framebuffer.h"

namespace libcamera {

using namespace icamera;
MockCameraHal::MockCameraHal(int32_t cameraId, PipelineHandler* pipe)
        : CameraHal(cameraId),
          mPipe(pipe) {
    LOG1("%s: camera %d", __func__, mCameraId);

    mFrameIntervalUs = 1000000 / 30;

    mFileSource = new icamera::FileSourceFromDir(PNP_INJECTION_NAME);
}

MockCameraHal::~MockCameraHal() {
    LOG1("%s: camera %d", __func__, mCameraId);
    Thread::requestExitAndWait();
    delete mFileSource;
}

int MockCameraHal::start() {
    LOG1("%s: ", __func__);
    std::string threadName = "MockCameraHAL";
    return run(threadName);
}

int MockCameraHal::stop() {
    LOG1("%s: ", __func__);
    Thread::requestExitAndWait();
    while (!mPendingRequests.empty()) {
        LOG1("%s:  size %d", __func__, mPendingRequests.size());
        Request* request = mPendingRequests.front();
        mPendingRequests.pop();
        completeRequest(request);
    }
    return 0;
}

int MockCameraHal::configure(icamera::stream_config_t* streamList) {
    return 0;
}

void MockCameraHal::processControls(Request* request, bool isStill) {
    UNUSED(isStill);
    if (!request) return;

    CameraHal::processControls(request);
    std::lock_guard<std::mutex> lock(mLock);
    mPendingRequests.push(request);
}

bool MockCameraHal::threadLoop() {
    if (!Thread::isExiting()) usleep(mFrameIntervalUs);

    Request* request = nullptr;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (!mPendingRequests.empty()) {
            request = mPendingRequests.front();
            mPendingRequests.pop();
        }
    }

    if (request) {
        LOG2("%s: request buffer size %d", __func__, request->buffers().size());
        completeRequest(request);
    }

    return true;
}

void MockCameraHal::completeRequest(Request* request) {
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    mTimestamp = (uint64_t)(t.tv_sec) * 1000000000UL + (uint64_t)(t.tv_nsec);
    ControlList metadata;
    metadata.set(controls::SensorTimestamp, mTimestamp);
    metadata.set(controls::draft::PipelineDepth, 7);
    metadata.set(controls::LensState, 0);
    for (auto buffer : request->buffers()) {
        const Stream* stream = buffer.first;
        const std::vector<FrameBuffer::Plane>& planes = buffer.second->planes();
        if (planes[0].fd.get() < 0) return;
        LOG2("%s: seq:%d", __func__, request->sequence());
        if (request->sequence() < kStartingFrameCount) {
            unsigned int frameSize = stream->configuration().frameSize;
            char* addr = reinterpret_cast<char*>(::mmap(nullptr, frameSize, PROT_READ | PROT_WRITE,
                                                        MAP_SHARED, planes[0].fd.get(), 0));
            if (mFileSource)
                mFileSource->fillFrameBuffer(addr, frameSize, request->sequence());
            else
                memset(addr, 0x99, frameSize);
            munmap(addr, frameSize);
        }
    }
    mPipe->completeMetadata(request, metadata);
    for (auto& buffer : request->buffers()) mPipe->completeBuffer(request, buffer.second);
    mPipe->completeRequest(request);
}

} /* namespace libcamera */
