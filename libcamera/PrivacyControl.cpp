/*
 * Copyright (C) 2024 Intel Corporation
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

#include "PrivacyControl.h"

#include <cros-camera/camera_buffer_manager.h>
#include <libcamera/base/log.h>
#include <sys/mman.h>

#include <algorithm>
#include <string>

namespace libcamera {
LOG_DEFINE_CATEGORY(IPU7Privacy)

PrivacyControl::PrivacyControl(int cameraId)
        : mCameraId(cameraId),
          mLastTimestamp(0L),
          mThreadRunning(false),
          mCallbackOps(nullptr) {
    LOG(IPU7Privacy, Debug) << "id " << std::to_string(mCameraId) << " " << __func__;
    mHwPrivacyControl = std::make_unique<HwPrivacyControl>(mCameraId);
    if (!mHwPrivacyControl->init()) {
        // init will fail on device doesn't have HW privacy control
        mHwPrivacyControl = nullptr;
    }
}

PrivacyControl::~PrivacyControl() {
    if (mHwPrivacyControl) mHwPrivacyControl->stop();
    mHwPrivacyControl = nullptr;
}

int PrivacyControl::start() {
    LOG(IPU7Privacy, Debug) << __func__;

    if (!mHwPrivacyControl) return NO_INIT;

    MutexLocker locker(mLock);
    mStreamQueueMap.clear();

    mThreadRunning = true;
    Thread::start();

    return mHwPrivacyControl->start();
}

int PrivacyControl::stop() {
    LOG(IPU7Privacy, Debug) << __func__;
    if (!mHwPrivacyControl) return NO_INIT;

    {
        MutexLocker locker(mLock);
        mThreadRunning = false;
        mRequestCondition.notify_one();
        while (mCaptureRequest.size() > 0) {
            mCaptureRequest.pop();
        }
    }
    wait();

    return icamera::OK;
}

int PrivacyControl::qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum) {
    LOG(IPU7Privacy, Debug) << __func__;
    std::shared_ptr<CaptureRequest> captureReq = std::make_shared<CaptureRequest>();
    captureReq->mBufferNum = bufferNum;
    for (int i = 0; i < bufferNum; i++) {
        captureReq->mBuffer[i] = ubuffer[i];
    }
    MutexLocker locker(mLock);
    mCaptureRequest.push(captureReq);
    if (mThreadRunning) {
        mRequestCondition.notify_one();
    }

    return icamera::OK;
}

int PrivacyControl::dqbuf(int streamId, icamera::camera_buffer_t** ubuffer) {
    LOG(IPU7Privacy, Debug) << __func__ << std::to_string(streamId);
    {
        MutexLocker locker(mLock);
        auto isReady =
            ([&]() LIBCAMERA_TSA_REQUIRES(mLock) { return !mStreamQueueMap[streamId].empty(); });

        auto duration = std::chrono::duration<float, std::ratio<2 / 1>>(5);
        while (mStreamQueueMap[streamId].empty()) {
            mResultCondition[streamId].wait_for(locker, duration, isReady);
        }
        *ubuffer = mStreamQueueMap[streamId].front();
        mStreamQueueMap[streamId].pop();
    }

    return icamera::OK;
}

bool PrivacyControl::getPrivacyMode() {
    if (mHwPrivacyControl) return mHwPrivacyControl->getPrivacyStatus();
    return false;
}

void PrivacyControl::run() {
    std::shared_ptr<CaptureRequest> captureReq = nullptr;
    /**
     * Set the initial timestamp of privacy mode to monotonic time which is same as physical
     * camera device
     */
    struct timespec t = {};
    clock_gettime(CLOCK_MONOTONIC, &t);
    mLastTimestamp = (uint64_t)(t.tv_sec) * 1000000000UL + (uint64_t)(t.tv_nsec) / 1000UL * 1000UL;

    while (true) {
        {
            MutexLocker locker(mLock);

            auto requestReady = ([&]() LIBCAMERA_TSA_REQUIRES(mLock) {
                return !mThreadRunning || !mCaptureRequest.empty();
            });

            auto duration = std::chrono::duration<float, std::ratio<2 / 1>>(5);
            if (mCaptureRequest.empty()) {
                mRequestCondition.wait_for(locker, duration, requestReady);
            }
            if (!mThreadRunning) return;
            if (mCaptureRequest.empty()) continue;

            captureReq = mCaptureRequest.front();
            mCaptureRequest.pop();
        }
        struct timeval curTime = {};
        gettimeofday(&curTime, nullptr);

        // Set the max fps in the range, todo read AE target from controls
        int frameInterval = 1000000.0 / 30;
        mLastTimestamp += frameInterval * 1000UL;

        uint32_t frameNumber = captureReq->mBuffer[0]->frameNumber;
        for (size_t i = 0; i < captureReq->mBufferNum; i++) {
            camera_buffer_t* buffer = captureReq->mBuffer[i];
            void* addr = (buffer->dmafd > 0)
                             ? ::mmap(nullptr, buffer->s.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                      buffer->dmafd, 0)
                             : buffer->addr;
            if (addr != MAP_FAILED) {
                // fill black feeds for all output buffers
                int offset = buffer->s.height * buffer->s.stride;
                memset(static_cast<char*>(addr), 16, offset);
                memset((static_cast<char*>(addr) + offset), 128, (buffer->s.size - offset));
                if (buffer->dmafd > 0) munmap(addr, buffer->s.size);
            }
        }

        if (mCallbackOps) {
            camera_msg_data_t data = {CAMERA_ISP_BUF_READY, {}};
            data.data.buffer_ready.timestamp = mLastTimestamp;
            data.data.buffer_ready.frameNumber = frameNumber;
            mCallbackOps->notify(mCallbackOps, data);

            data = {CAMERA_METADATA_READY, {}};
            data.data.metadata_ready.sequence = -1;
            data.data.metadata_ready.frameNumber = frameNumber;
            mCallbackOps->notify(mCallbackOps, data);
        }

        struct timeval tmpCurTime = {};
        gettimeofday(&tmpCurTime, nullptr);
        int duration = static_cast<int>(tmpCurTime.tv_usec - curTime.tv_usec +
                                        ((tmpCurTime.tv_sec - curTime.tv_sec) * 1000000UL));
        LOG(IPU7Privacy, Debug) << "sleep to wait FPS duration: "
                                << std::to_string(frameInterval - duration);

        if (frameInterval - duration < 0) {
            LOG(IPU7Privacy, Debug) << "take too long to fill the buffers, skip sleep";
        } else {
            usleep(frameInterval - duration);
        }

        for (size_t i = 0; i < captureReq->mBufferNum; i++) {
            int streamId = captureReq->mBuffer[i]->s.id;
            mResultCondition[streamId].notify_one();
            if (mCallbackOps) {
                camera_msg_data_t data = {CAMERA_FRAME_DONE, {}};
                data.data.frame_ready.streamId = streamId;
                mCallbackOps->notify(mCallbackOps, data);
            }
            MutexLocker locker(mLock);
            mStreamQueueMap[streamId].push(captureReq->mBuffer[i]);
            mResultCondition[streamId].notify_one();
        }
    }
}

void PrivacyControl::updateMetadataResult(ControlList& metadata) {
    metadata.set(controls::LensState, 0);
}
}  // namespace libcamera
