/*
 * Copyright (C) 2022 Intel Corporation.
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

#define LOG_TAG FaceStage

#include "FaceStage.h"
#include "iutils/CameraLog.h"
#include "PlatformData.h"
#include "CameraContext.h"

using std::shared_ptr;

namespace icamera {

FaceStage::FaceStage(int cameraId, int streamId, const stream_t& stream, bool isPrivate)
        : CameraStream(cameraId, streamId, stream),
          ISchedulerNode("face"),
          mStreamInfo(stream),
          mInternalBufferPool(nullptr),
          mIsPrivate(isPrivate),
          mFaceDetection(nullptr) {
    LOG1("%s, mIsPrivate: %d, width: %d, height: %d", __func__, mIsPrivate,
         mStreamInfo.width, mStreamInfo.height);

    if (mIsPrivate || (!mIsPrivate && !PlatformData::runFaceWithSyncMode(mCameraId))) {
        mInternalBufferPool = std::unique_ptr<CameraBufferPool>(new CameraBufferPool());
        mInternalBufferPool->createBufferPool(mCameraId, MAX_BUFFER_COUNT, mStreamInfo);
    }
    mFaceDetection = std::unique_ptr<FaceDetection>(
            IFaceDetection::createFaceDetection(mCameraId, stream.width, stream.height));
}

FaceStage::~FaceStage() {
    LOG1("%s, mIsPrivate: %d", __func__, mIsPrivate);
}

int FaceStage::start() {
    LOG1("<id%d>@%s, %p, mIsPrivate: %d", mCameraId, __func__, this, mIsPrivate);

    return OK;
}

int FaceStage::stop() {
    CameraStream::stop();

    if (mInternalBufferPool) {
        mInternalBufferPool->destroyBufferPool();
    }

    return OK;
}

// Q buffers to the stream processor which should be set by the CameraDevice
int FaceStage::qbuf(camera_buffer_t* ubuffer, int64_t sequence, bool addExtraBuf) {
    LOG2("<seq:%ld>, %s, mIsPrivate: %d, addExtraBuf: %d", sequence, __func__,
         mIsPrivate, addExtraBuf);

    int ret = BAD_VALUE;
    if (mIsPrivate) {
        shared_ptr<CameraBuffer> camBuffer = nullptr;
        if (addExtraBuf) {
            // use the internal buffer pool for private stream
            CheckAndLogError(!mInternalBufferPool, ret, "no buffer pool for private stream");

            camBuffer = mInternalBufferPool->acquireBuffer();
            CheckAndLogError(!camBuffer, ret, "%s, No available internal buffer", __func__);
            LOG2("<id%d:seq%ld>@%s, mStreamId:%d, CameraBuffer:%p for port:%d, addr:%p",
                 mCameraId, sequence, __func__, mStreamId, camBuffer.get(), mPort,
                 camBuffer->getBufferAddr());

            camBuffer->setSequence(0);
            struct timeval ts = {0};
            camBuffer->setTimestamp(ts);

            camBuffer->setSettingSequence(sequence);
        }

        if (mBufferProducer != nullptr) {
            ret = mBufferProducer->qbuf(mPort, camBuffer);
            if (ret == OK) {
                AutoMutex l(mBufferPoolLock);
                mBufferInProcessing++;
                LOG2("%s, mIsPrivate: %d, buffer in processing: %d", __func__, mIsPrivate,
                     mBufferInProcessing);
            }
        }
    } else {
        // use normal preview stream for face detection
        ret = CameraStream::qbuf(ubuffer, sequence);
    }

    return ret;
}

shared_ptr<CameraBuffer> FaceStage::copyToInternalBuffer(
        const shared_ptr<CameraBuffer>& camBuffer) {
    CheckAndLogError(!mInternalBufferPool, nullptr,
                     "%s, no buffer pool for face detection", __func__);

    shared_ptr<CameraBuffer> faceBuffer = mInternalBufferPool->acquireBuffer();
    CheckAndLogError(!faceBuffer, nullptr, "%s, No available internal buffer", __func__);

#ifdef CAL_BUILD
    // Need to lock for gbm buffer from user
    if (camBuffer->getMemory() == V4L2_MEMORY_DMABUF) {
        camBuffer->lock();
    }
#endif
    CheckAndLogError(!camBuffer->getBufferAddr(), nullptr,
                     "%s, Failed to get addr for camBuffer", __func__);

    MEMCPY_S(faceBuffer->getBufferAddr(), faceBuffer->getBufferSize(),
             camBuffer->getBufferAddr(), camBuffer->getBufferSize());

#ifdef CAL_BUILD
    if (camBuffer->getMemory() == V4L2_MEMORY_DMABUF) {
        camBuffer->unlock();
    }
#endif

    return faceBuffer;
}

bool FaceStage::isFaceEnabled(int64_t sequence) {
    auto cameraContext = CameraContext::getInstance(mCameraId);
    auto dataContext = cameraContext->getDataContextBySeq(sequence);
    if (dataContext->mFaceDetectMode != CAMERA_STATISTICS_FACE_DETECT_MODE_OFF ||
        PlatformData::isFaceAeEnabled(mCameraId))
        return true;

    return false;
}

int FaceStage::onFrameAvailable(uuid port, const shared_ptr<CameraBuffer>& camBuffer) {
    // Ignore if the buffer is not for this stream.
    if (mPort != port) return OK;

    int64_t sequence = camBuffer->getSequence();
    if (mIsPrivate) {
        if (isFaceEnabled(sequence) && mFaceDetection->needRunFace(sequence) &&
            !PlatformData::runFaceWithSyncMode(mCameraId)) {
            AutoMutex l(mBufferPoolLock);
            LOG2("<seq%ld>%s, run face with ASYNC mode. mIsPrivate: %d",
                 sequence, __func__, mIsPrivate);
            mPendingBufferQ.push(camBuffer);
            return OK;
        }

        if (isFaceEnabled(sequence) && mFaceDetection->needRunFace(sequence)) {
            LOG2("<seq%ld>%s, run face with SYNC mode. mIsPrivate: %d",
                 sequence, __func__, mIsPrivate);
            mFaceDetection->runFaceDetection(camBuffer);
        }

        AutoMutex l(mBufferPoolLock);
        if (mBufferInProcessing > 0) mBufferInProcessing--;
        if (mInternalBufferPool) mInternalBufferPool->returnBuffer(camBuffer);
    } else {
        if (isFaceEnabled(sequence) && mFaceDetection->needRunFace(sequence)) {
            if (PlatformData::runFaceWithSyncMode(mCameraId)) {
                LOG2("<seq%ld>%s, run face with SYNC mode. mIsPrivate: %d",
                     sequence, __func__, mIsPrivate);
                mFaceDetection->runFaceDetection(camBuffer);
            } else {
                LOG2("<seq%ld>%s, run face with ASYNC mode. mIsPrivate: %d",
                     sequence, __func__, mIsPrivate);

                shared_ptr<CameraBuffer> faceBuffer = copyToInternalBuffer(camBuffer);
                CheckAndLogError(!faceBuffer, UNKNOWN_ERROR,
                        "        %s, Failed to copy frame to internal buffer", __func__);
                AutoMutex l(mBufferPoolLock);
                mPendingBufferQ.push(faceBuffer);
            }
        }
        CameraStream::onFrameAvailable(port, camBuffer);
    }
    return OK;
}

bool FaceStage::process(int64_t triggerId) {
    PERF_CAMERA_ATRACE_PARAM1(getName(), triggerId);
    LOG2("%s, triggerId: %ld. mPrivate: %d", __func__, triggerId, mIsPrivate);
    shared_ptr<CameraBuffer> faceBuffer = nullptr;
    {
        AutoMutex l(mBufferPoolLock);
        if (mPendingBufferQ.empty()) return true;
        faceBuffer = mPendingBufferQ.front();
        mPendingBufferQ.pop();

        if (mPendingBufferQ.size() > PlatformData::getMaxRequestsInflight(mCameraId)) {
            LOG2("%s, Skip this time due to many buffer in pendding: %d", __func__,
                 mPendingBufferQ.size());

            if (mIsPrivate && mBufferInProcessing > 0) mBufferInProcessing--;
            if (mInternalBufferPool) mInternalBufferPool->returnBuffer(faceBuffer);
            return true;
        }
    }
    CheckAndLogError(!faceBuffer, false, "%s, the faceBuffer is nullptr", __func__);

    LOG2("<seq%ld>%s, run face detection. triggerId: %ld. mPrivate: %d",
         faceBuffer->getSequence(), __func__, triggerId, mIsPrivate);
    mFaceDetection->runFaceDetection(faceBuffer);

    AutoMutex l(mBufferPoolLock);
    if (mIsPrivate && mBufferInProcessing > 0) mBufferInProcessing--;
    if (mInternalBufferPool) mInternalBufferPool->returnBuffer(faceBuffer);

    return true;
}
}  // namespace icamera
