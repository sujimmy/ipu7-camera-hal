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
#define LOG_TAG Camera3HAL

#include <memory>
#include <vector>

#include <queue>

#include <libcamera/base/log.h>
#include <libcamera/base/mutex.h>
#include <libcamera/base/utils.h>

#include <libcamera/camera.h>
#include <libcamera/control_ids.h>
#include <libcamera/formats.h>
#include <libcamera/property_ids.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include "libcamera/internal/camera.h"
#include "libcamera/internal/device_enumerator.h"
#include "libcamera/internal/media_device.h"
#include "libcamera/internal/pipeline_handler.h"
#include <libcamera/internal/formats.h>

#include "Frames.h"
#ifdef IPA_SANDBOXING
#include "IPAClient.h"
#include "IGPUIPAClient.h"
#endif
#include "ParameterConverter.h"

#include "CameraDevice.h"
#include "Camera3AMetadata.h"
#include "PrivacyControl.h"
#include "PlatformData.h"
#include "MediaControl.h"
#include "CameraContext.h"

namespace libcamera {

LOG_DEFINE_CATEGORY(IPU7)

#define UNUSED(param) (void)(param)

using namespace icamera;

class IPU7CameraData : public Camera::Private {
 public:
    IPU7CameraData(PipelineHandler* pipe, int32_t cameraId);
    ~IPU7CameraData();

    static void setup();
    static void tearDown();

    void initializeCapabilities();
    void initProperties();
    std::vector<SizeRange> availableStreamSizes(const PixelFormat &pixelFormat) const;

    void start();
    void stop();

    void connectCallback();
    void handleNewRequest(Request* request);
    void processNewRequest();

    void returnRequestDone(unsigned int frameNumber);

    // Device operation
    bool validate(const icamera::stream_t& stream);
    int configure(icamera::stream_config_t *streamList);
    void processControls(Request* request, bool isStill = false);
    void updateMetadataResult(int64_t sequence, const ControlList& controls,
                              ControlList& metadata);
    int qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum);
    int dqbuf(int streamId, icamera::camera_buffer_t** ubuffer);

 public:
    int32_t mCameraId;
    /* based on current libcamera design, define Pipeline output stream numbers
    *  to 6, 3 video, 2 still, 1 raw.
    */
    static const int8_t kVideoStreamNum = 3;
    static const int8_t kStillStreamNum = 2;
    static const int8_t kMaxStreamNum = kVideoStreamNum + kStillStreamNum + 1;
    Stream mVideoStreams[kVideoStreamNum];
    Stream mStillStreams[kStillStreamNum];
    Stream mRawStream;

    IPUFrames mFrameInfo;
    icamera::stream_config_t mStreamList;
    std::map<Stream*, unsigned int> mStreamToStreamIndexMap;

 private:
    void metadataReady(unsigned int frameNumber, int64_t sequence);
    void bufferReady(unsigned int streamId);
    void shutterReady(unsigned int frameNumber, int64_t timestamp);

    void processPrivacySwitch();
    void waitAllRequestsDone();

 private:
    icamera::stream_t mStreams[kMaxStreamNum];
    struct CameraBufferInfo {
        icamera::camera_buffer_t halBuffer[kMaxStreamNum];
        uint32_t frameNumber;
        bool frameInProcessing;
    };
    static const int8_t kMaxProcessRequestNum = 10;
    CameraBufferInfo mCameraBufferInfo[kMaxProcessRequestNum];

    mutable Mutex mMutex;
    std::queue<Request*> mPendingRequests;

    // Device data
    icamera::CameraDevice* mCameraDevice;
    Camera3AMetadata* mCamera3AMetadata;
    bool mCameraStarted;
    std::map<int32_t, int32_t> mRequestInProgress;
    std::condition_variable mRequestCondition;
    std::mutex mLock;

    icamera::stream_config_t mStreamConfig;
    std::unique_ptr<PrivacyControl> mPrivacyControl;
    bool mPrivacyStarted;
};

class IPU7CameraConfiguration : public CameraConfiguration {
 public:
    IPU7CameraConfiguration(IPU7CameraData* data);

    Status validate() override;

 private:
    const unsigned int kMaxBufferCount = 6;
    IPU7CameraData* mData;
};

class PipelineHandlerIPU7 : public PipelineHandler {
 public:
    PipelineHandlerIPU7(CameraManager* manager);
    virtual ~PipelineHandlerIPU7();

    std::unique_ptr<CameraConfiguration>
    generateConfiguration(Camera* camera, Span<const StreamRole> roles) override;
    int configure(Camera* camera, CameraConfiguration* config) override;

    int exportFrameBuffers(Camera* camera, Stream* stream,
                           std::vector<std::unique_ptr<FrameBuffer>>* buffers) override;

    int start(Camera* camera, const ControlList* controls) override;
    void stopDevice(Camera* camera) override;

    int queueRequestDevice(Camera* camera, Request* request) override;

    bool match(DeviceEnumerator* enumerator) override;

 private:
    IPU7CameraData *cameraData(Camera* camera) {
        return static_cast<IPU7CameraData*>(camera->_d());
    }

    int registerCameras();

 private:
    // hal setup done and cameras registered
    bool mHalInitialized;
};

IPU7CameraData::IPU7CameraData(PipelineHandler* pipe, int32_t cameraId)
    : Camera::Private(pipe),
      mCameraId(cameraId),
      mCameraStarted(false),
      mPrivacyStarted(false) {
    LOG(IPU7, Info) << "<id" << mCameraId << ">@" << __func__;

    memset(&mStreamList, 0, sizeof(mStreamList));
    mStreamList.streams = mStreams;
    memset(&mCameraBufferInfo, 0, sizeof(mCameraBufferInfo));

    icamera::MediaControl* mc = icamera::MediaControl::getInstance();
    if (!mc) {
        LOG(IPU7, Error) << "MediaControl init failed";
        return;
    }
    mc->resetAllLinks();

    icamera::CameraContext::getInstance(mCameraId);

    mCameraDevice = new icamera::CameraDevice(mCameraId);
    int ret = mCameraDevice->init();
    if (ret != icamera::OK) {
        LOG(IPU7, Error) << "CameraDevice init failed, ret: " << ret;
        mCameraDevice->deinit();
        return;
    }
    mCamera3AMetadata = new Camera3AMetadata(mCameraId);
    mPrivacyControl = std::make_unique<PrivacyControl>(mCameraId);
    mPrivacyStarted = mPrivacyControl->getPrivacyMode();

    mPrivacyControl->callbackRegister(&mFrameInfo.mResultsHandler);
    mCameraDevice->callbackRegister(&mFrameInfo.mResultsHandler);
}

IPU7CameraData::~IPU7CameraData(){
    LOG(IPU7, Info) << "<id" << mCameraId << ">@" << __func__;

    if (mCameraDevice) {
        mCameraDevice->deinit();
        delete mCameraDevice;
    }

    if (mCamera3AMetadata) delete mCamera3AMetadata;
    mPrivacyControl = nullptr;

    icamera::CameraContext::releaseInstance(mCameraId);
}

void IPU7CameraData::initializeCapabilities() {
    ControlInfoMap::Map controls{};
    ParameterConverter::initializeCapabilities(mCameraId, properties_, controls);

    controlInfo_ = ControlInfoMap(std::move(controls), controls::controls);
}

void IPU7CameraData::initProperties() {
    ParameterConverter::initProperties(mCameraId, properties_);
}

void IPU7CameraData::start() {
}

void IPU7CameraData::stop() {
    // mCameraStarted is accessed only in capture thread, no need to lock
    if (!mCameraStarted) return;

    mCameraStarted = false;
    if (mPrivacyStarted)
        mPrivacyControl->stop();
    else
        mCameraDevice->stop();
}

void IPU7CameraData::handleNewRequest(Request* request) {
    processPrivacySwitch();
    {
    MutexLocker locker(mMutex);

    mPendingRequests.push(request);
    }

    processNewRequest();
}

void IPU7CameraData::processNewRequest() {
    MutexLocker locker(mMutex);

    if (mPendingRequests.empty()) return;

    int8_t index = -1;
    for (int8_t i = 0; i < kMaxProcessRequestNum; i++) {
        if (!mCameraBufferInfo[i].frameInProcessing) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        LOG(IPU7, Warning) << "No empty CameraBufferInfo is available";
        return;
    }

    Request* request = mPendingRequests.front();
    Info* info = mFrameInfo.create(request);
    if (!info) {
        LOG(IPU7, Warning) << "Failed to create Info";
        return;
    }

    bool isStill = false;
    for (int i = 0; i < kStillStreamNum; i++) {
        if (request->findBuffer(&mStillStreams[i]) != nullptr) {
            isStill = true;
            break;
        }
    }
    processControls(request, isStill);

    icamera::camera_buffer_t* halBuffer[kMaxStreamNum] = {nullptr};
    int8_t bufferNum = 0;
    for (auto const &buffer : request->buffers()) {
        LOG(IPU7, Debug) << " request stream " << buffer.first;
        Stream* usrStream = const_cast<Stream*>(buffer.first);
        unsigned int id = mStreamToStreamIndexMap[usrStream];
        icamera::stream_t halStream = mStreamList.streams[id];

        mFrameInfo.getBuffer(info, halStream, buffer.second,
                              &mCameraBufferInfo[index].halBuffer[bufferNum]);
        mCameraBufferInfo[index].halBuffer[bufferNum].sequence = -1;
        mCameraBufferInfo[index].halBuffer[bufferNum].timestamp = 0;
        halBuffer[bufferNum] = &mCameraBufferInfo[index].halBuffer[bufferNum];
        bufferNum++;
    }

    int ret = qbuf(halBuffer, bufferNum);
    if (ret != 0) {
        LOG(IPU7, Error) << "Failed to queue buffers";
        return;
    }

    mCameraBufferInfo[index].frameInProcessing = true;
    mCameraBufferInfo[index].frameNumber = request->sequence();

    mPendingRequests.pop();

    LOG(IPU7, Debug) << " request processing " << info->id;
}

void IPU7CameraData::connectCallback() {
    LOG(IPU7, Info) << "IPU7CameraData::connectCallback()";

    mFrameInfo.mResultsHandler.mMetadataAvailable.connect(this, &IPU7CameraData::metadataReady);
    mFrameInfo.mResultsHandler.mBufferAvailable.connect(this, &IPU7CameraData::bufferReady);
    mFrameInfo.mResultsHandler.mShutterReady.connect(this, &IPU7CameraData::shutterReady);
}

void IPU7CameraData::returnRequestDone(unsigned int frameNumber) {
    Info* info = mFrameInfo.requestComplete(frameNumber);
    if (!info) return;

    mFrameInfo.remove(info);

    pipe()->completeRequest(info->request);

    for (int8_t i = 0; i < kMaxProcessRequestNum; i++) {
        if (frameNumber == mCameraBufferInfo[i].frameNumber) {
            memset(&mCameraBufferInfo[i], 0, sizeof(mCameraBufferInfo[i]));
            break;
        }
    }

    LOG(IPU7, Debug) << " request done " << info->id << " frameNumber " << frameNumber;
    processNewRequest();
}

void IPU7CameraData::metadataReady(unsigned int frameNumber, int64_t sequence) {
    Info* info = mFrameInfo.find(frameNumber);
    ControlList metadata;
    if (info) {
        updateMetadataResult(sequence, info->request->controls(), metadata);
        pipe()->completeMetadata(info->request, metadata);
    }

    mFrameInfo.metadataReady(frameNumber, sequence);

    returnRequestDone(frameNumber);
}

void IPU7CameraData::bufferReady(unsigned int streamId) {
    icamera::camera_buffer_t* buffer = nullptr;

    int ret = dqbuf(streamId, &buffer);
    if (ret != 0) {
        LOG(IPU7, Error) << "Failed to dequeue buffer";
        return;
    }

    Info* info = mFrameInfo.find(buffer->frameNumber);
    if (info && info->outBuffers.find(streamId) != info->outBuffers.end()) {
        FrameBuffer* frameBuffer = info->outBuffers[streamId];
        pipe()->completeBuffer(info->request, frameBuffer);

        mFrameInfo.bufferReady(buffer->frameNumber, streamId);

        returnRequestDone(buffer->frameNumber);
    }
}

void IPU7CameraData::shutterReady(unsigned int frameNumber, int64_t timestamp) {
    Info* info = mFrameInfo.find(frameNumber);
    ControlList metadata;
    if (info) {
        metadata.set(controls::SensorTimestamp, timestamp);
        pipe()->completeMetadata(info->request, metadata);
    }

    mFrameInfo.shutterReady(frameNumber);
}

void IPU7CameraData::setup() {
    LOG(IPU7, Info) << __func__;
    icamera::Log::setDebugLevel();
    icamera::CameraDump::setDumpLevel();
    icamera::PlatformData::init();
}

void IPU7CameraData::tearDown() {
    LOG(IPU7, Info) << __func__;
    icamera::PlatformData::releaseInstance();
}

bool IPU7CameraData::validate(const icamera::stream_t& stream) {
    return PlatformData::isSupportedStream(mCameraId, stream);
}

std::vector<SizeRange> IPU7CameraData::availableStreamSizes(const PixelFormat &pixelFormat) const {
    auto staticMetadata = PlatformData::getStaticMetadata(mCameraId);
    std::vector<SizeRange> sizes;
    if (staticMetadata && staticMetadata->mConfigsArray.size() > 0) {
        // currently only NV12 format output is supported, SGRBG10 not supported in config file
        int streamFormat =
            pixelFormat == formats::SGRBG10 ? V4L2_PIX_FMT_SGBRG10 : V4L2_PIX_FMT_NV12;
        for (auto& stream : staticMetadata->mConfigsArray) {
            if (stream.format == streamFormat) {
                // size is sorted in descending order in config file
                sizes.emplace_back(Size(stream.width, stream.height));
            }
        }
    }

    return sizes;
}

int IPU7CameraData::configure(icamera::stream_config_t *streamList) {

    mPrivacyControl->configure(streamList);
    mStreamConfig = *streamList;

    return mCameraDevice->configure(streamList);
}

void IPU7CameraData::processControls(Request* request, bool isStill) {
    if (!request || mPrivacyStarted) return;

    auto cameraContext = icamera::CameraContext::getInstance(mCameraId);
    icamera::DataContext* dataContext = cameraContext->acquireDataContext();

    cameraContext->updateDataContextMapByFn(request->sequence(), dataContext);

    dataContext->mAiqParams.makernoteMode = icamera::MAKERNOTE_MODE_OFF;
    if (isStill)
        dataContext->mAiqParams.frameUsage = icamera::FRAME_USAGE_STILL;
    else
        dataContext->mAiqParams.frameUsage = icamera::FRAME_USAGE_PREVIEW;

    ParameterConverter::controls2DataContext(mCameraId, request->controls(), dataContext);
}

void IPU7CameraData::updateMetadataResult(int64_t sequence, const ControlList& controls,
                                          ControlList& metadata) {
    if (mPrivacyStarted) return mPrivacyControl->updateMetadataResult(metadata);

    auto cameraContext = icamera::CameraContext::getInstance(mCameraId);
    auto dataContext = cameraContext->getDataContextBySeq(sequence);
    auto resultStorage = cameraContext->getAiqResultStorage();
    const AiqResult* aiqResult = resultStorage->getAiqResult(sequence);
    const auto faceResult = resultStorage->getFaceResult();
    mCamera3AMetadata->process3Astate(aiqResult, dataContext, controls, metadata);

    ParameterConverter::dataContext2Controls(mCameraId, dataContext, faceResult, aiqResult,
                                             metadata);
}

int IPU7CameraData::qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum) {
    {
        std::unique_lock<std::mutex> lock(mLock);
        for (int i = 0; i < bufferNum; i++) {
            int streamId = ubuffer[i]->s.id;
            mRequestInProgress[streamId]++;
        }
    }

    int ret = OK;
    if (mPrivacyStarted) ret = mPrivacyControl->qbuf(ubuffer, bufferNum);
    else ret = mCameraDevice->qbuf(ubuffer, bufferNum);
    // Start camera after the first buffer queued
    if (!mCameraStarted) {
        if (mPrivacyStarted) mPrivacyControl->start();
        else mCameraDevice->start();
        mCameraStarted = true;
    }

    return ret;
}

int IPU7CameraData::dqbuf(int streamId, icamera::camera_buffer_t** ubuffer) {
    int ret = OK;
    if (mPrivacyStarted) ret = mPrivacyControl->dqbuf(streamId, ubuffer);
    else ret = mCameraDevice->dqbuf(streamId, ubuffer);

    {
        std::unique_lock<std::mutex> lock(mLock);
        mRequestInProgress[streamId]--;
        mRequestCondition.notify_one();
    }

    return ret;
}

void IPU7CameraData::waitAllRequestsDone() {
    std::unique_lock<std::mutex> lock(mLock);
    for ( auto& it : mRequestInProgress) {
        while (it.second > 0) {
            auto duration = std::chrono::duration<float, std::ratio<2 / 1>>(10);
            mRequestCondition.wait_for(lock, duration);
        }
    }
}

void IPU7CameraData::processPrivacySwitch() {
    bool needSwitch = mPrivacyStarted != mPrivacyControl->getPrivacyMode();
    if (!needSwitch) return;
    LOG(IPU7, Debug) << __func__;
    waitAllRequestsDone();

    if (mPrivacyStarted) {
        mPrivacyControl->stop();
    } else {
        mCameraDevice->stop();
        mCameraDevice->configure(&mStreamConfig);
    }

    mCameraStarted = false;
    /* mPrivacyStarted only written here, at this time, the qbuf/start/stop is blocked, and all
    ** requests dqbuf are done. locked is not needed
    */
    mPrivacyStarted = !mPrivacyStarted;
}

IPU7CameraConfiguration::IPU7CameraConfiguration(IPU7CameraData* data)
    : CameraConfiguration() {
    LOG(IPU7, Info) << "IPU7CameraConfiguration()";
    mData = data;
}

CameraConfiguration::Status IPU7CameraConfiguration::validate() {
    LOG(IPU7, Info) << "IPU7CameraConfiguration::validate()";
    if (config_.empty() || config_.size() > IPU7CameraData::kMaxStreamNum) return Invalid;

    int videoStreamIndex = 0;
    int stillStreamIndex = 0;
    for (unsigned int i = 0; i < config_.size(); ++i) {
        const StreamConfiguration originalCfg = config_[i];
        StreamConfiguration *cfg = &config_[i];
        switch (cfg->role) {
        case StreamRole::StillCapture:
            if (stillStreamIndex >= IPU7CameraData::kStillStreamNum) return Invalid;
            cfg->setStream(&(mData->mStillStreams[stillStreamIndex]));
            stillStreamIndex++;
            break;
        case StreamRole::VideoRecording:
        case StreamRole::Viewfinder:
            if (videoStreamIndex >= IPU7CameraData::kVideoStreamNum) return Invalid;
            cfg->setStream(&(mData->mVideoStreams[videoStreamIndex]));
            videoStreamIndex++;
            break;
        case StreamRole::Raw:
            cfg->setStream(&(mData->mRawStream));
            break;
        }

        const PixelFormatInfo &info = PixelFormatInfo::info(config_[i].pixelFormat);
        if (info.colourEncoding == PixelFormatInfo::ColourEncodingRAW) {
            cfg->pixelFormat = formats::SGRBG10;
            cfg->stride = info.stride(cfg->size.width, 0, 64);
            cfg->frameSize = info.frameSize(cfg->size, 64);
            // TODO get buffer count from HAL
            cfg->bufferCount = kMaxBufferCount;
        } else {
            cfg->pixelFormat = formats::NV12;
            cfg->bufferCount = kMaxBufferCount;
            cfg->stride = info.stride(cfg->size.width, 0, 1);
            cfg->frameSize = info.frameSize(cfg->size, 1);
        }

        icamera::stream_t stream = {};
        stream.width = cfg->size.width;
        stream.height = cfg->size.height;
        stream.format = cfg->pixelFormat.fourcc();
        stream.field = 0;
        if (!mData->validate(stream)) return Invalid;

        LOG(IPU7, Info) << " stream " << cfg->toString() << " Valid";
    }

    return Valid;
}

PipelineHandlerIPU7::PipelineHandlerIPU7(CameraManager* manager)
    : PipelineHandler(manager),
      mHalInitialized(false) {
    LOG(IPU7, Info) << __FUNCTION__ << " Construct " << this;
}

PipelineHandlerIPU7::~PipelineHandlerIPU7() {
    LOG(IPU7, Info) << __FUNCTION__ << " Destroy " << this;
    if (mHalInitialized) {
#ifdef IPA_SANDBOXING
        IPAClient::removeInstance();
#endif

        IPU7CameraData::tearDown();
    }
}

std::unique_ptr<CameraConfiguration>
PipelineHandlerIPU7::generateConfiguration(Camera* camera, Span<const StreamRole> roles) {
    LOG(IPU7, Info) << "PipelineHandlerIPU7::generateConfiguration()";
    IPU7CameraData *data = cameraData(camera);
    std::unique_ptr<IPU7CameraConfiguration> config =
        std::make_unique<IPU7CameraConfiguration>(data);

    if (roles.empty()) return config;

    for (const StreamRole role : roles) {
        std::map<PixelFormat, std::vector<SizeRange>> streamFormats;
        PixelFormat pixelFormat = (role == StreamRole::Raw) ? formats::SGRBG10 : formats::NV12;
        std::vector<SizeRange> sizes = data->availableStreamSizes(pixelFormat);
        if (sizes.size() == 0) return {};
        streamFormats[pixelFormat] = sizes;
        StreamFormats formats(streamFormats);
        StreamConfiguration cfg(formats);
        // set default size to max size
        cfg.size = sizes.at(0).max;
        cfg.pixelFormat = pixelFormat;
        cfg.role = role;
        config->addConfiguration(cfg);
        LOG(IPU7, Debug) << "generate config " << pixelFormat << " size " << cfg.toString();
    }

    if (config->validate() == CameraConfiguration::Invalid) return {};

    LOG(IPU7, Debug) << "generateConfiguration done, config count " << config->size();
    return config;
}

int PipelineHandlerIPU7::configure(Camera* camera, CameraConfiguration* c) {
    // do not call camera device config durning match() configuration
    // Fix me, needed add PipelineHandler CameraConfiguration check here
    if (!mHalInitialized) return 0;

    IPU7CameraConfiguration *config =
        static_cast<IPU7CameraConfiguration *>(c);
    IPU7CameraData *data = cameraData(camera);
    data->mStreamList.num_streams = config->size();

    for (unsigned int i = 0; i < config->size(); ++i) {
        StreamConfiguration &cfg = config->at(i);
        data->mStreamList.streams[i].width = cfg.size.width;
        data->mStreamList.streams[i].height = cfg.size.height;
        data->mStreamList.streams[i].stride = cfg.stride;
        data->mStreamList.streams[i].size = cfg.frameSize;
        data->mStreamList.streams[i].field = 0;
        data->mStreamList.streams[i].format = cfg.pixelFormat.fourcc();
        data->mStreamList.streams[i].max_buffers = cfg.bufferCount;
        data->mStreamList.streams[i].id = i;
        data->mStreamList.streams[i].streamType = icamera::CAMERA_STREAM_OUTPUT;
        LOG(IPU7, Info) << "configure(): " << cfg.toString() << " role " << cfg.role;

        switch (cfg.role) {
            case StreamRole::StillCapture:
                data->mStreamList.streams[i].usage = icamera::CAMERA_STREAM_STILL_CAPTURE;
                break;
            case StreamRole::VideoRecording:
                data->mStreamList.streams[i].usage = icamera::CAMERA_STREAM_VIDEO_CAPTURE;
                break;
            case StreamRole::Viewfinder:
                data->mStreamList.streams[i].usage = icamera::CAMERA_STREAM_PREVIEW;
                break;
            case StreamRole::Raw:
                data->mStreamList.streams[i].usage = icamera::CAMERA_STREAM_OPAQUE_RAW;
                break;
        }

        data->mStreamList.streams[i].memType = V4L2_MEMORY_USERPTR;

        LOG(IPU7, Debug) << " stream " << cfg.stream() << " size: " << cfg.toString();

        data->mStreamToStreamIndexMap[cfg.stream()] = i;
    }
    if (data->mStreamList.num_streams > 0) data->configure(&data->mStreamList);

    return 0;
}

int PipelineHandlerIPU7::exportFrameBuffers(Camera* camera, Stream* stream,
                                            std::vector<std::unique_ptr<FrameBuffer>> *buffers) {
    LOG(IPU7, Info) << "exportFrameBuffers()";
    UNUSED(camera);
    UNUSED(stream);
    UNUSED(buffers);
    return 0;
}

int PipelineHandlerIPU7::start(Camera* camera, [[maybe_unused]] const ControlList* controls) {
    LOG(IPU7, Info) << "PipelineHandlerIPU7::start()";

    IPU7CameraData *data = cameraData(camera);
    data->start();
    return 0;
}

void PipelineHandlerIPU7::stopDevice(Camera* camera) {
    LOG(IPU7, Info) << "PipelineHandlerIPU7::stopDevice()";

    IPU7CameraData *data = cameraData(camera);
    data->stop();
}

int PipelineHandlerIPU7::queueRequestDevice(Camera* camera, Request* request) {
    LOG(IPU7, Debug) << "PipelineHandlerIPU7::queueRequestDevice()";

    IPU7CameraData *data = cameraData(camera);
    data->handleNewRequest(request);
    return 0;
}

bool PipelineHandlerIPU7::match(DeviceEnumerator* enumerator) {
    LOG(IPU7, Info) << "PipelineHandlerIPU7::match()";

    DeviceMatch isysDm("intel-ipu6-isys");
    MediaDevice* isysDev = acquireMediaDevice(enumerator, isysDm);
    if (!isysDev) return false;

    IPU7CameraData::setup();

#ifdef IPA_SANDBOXING
    IPAClient::createInstance(this);
    bool fine = IPAClient::getInstance()->isIPAFine();
    if (fine) {
        LOG(IPU7, Info) << "IPA connection is successful";
    } else {
        LOG(IPU7, Error) << "IPA connection is not successful";
        return false;
    }

    if (icamera::PlatformData::isUsingGpuIpa()) {
        IGPUIPAClient::createInstance(this);
        fine = IGPUIPAClient::getInstance()->isIPAFine();
        if (fine) {
            LOG(IPU7, Info) << "Connected to GPU IPA";
        } else {
            LOG(IPU7, Error) << "GPU IPA connection failed";
            IGPUIPAClient::removeInstance();
            return false;
        }
    }
#endif

    int ret = registerCameras();
    mHalInitialized = true;

    return ret == 0;
}

int PipelineHandlerIPU7::registerCameras() {
    unsigned int numCameras = 0;

    const int32_t cameraId = 0;
    const std::string cameraName = std::string("camera") + std::to_string(cameraId);
    std::unique_ptr<IPU7CameraData> data = std::make_unique<IPU7CameraData>(this, cameraId);
    data->initProperties();
    data->initializeCapabilities();

    data->connectCallback();

    std::set<Stream*> streams;
    for (int i = 0; i < IPU7CameraData::kVideoStreamNum; i++) {
        streams.insert(&(data->mVideoStreams[i]));
    }
    for (int i = 0; i < IPU7CameraData::kStillStreamNum; i++) {
        streams.insert(&(data->mStillStreams[i]));
    }
    streams.insert(&(data->mRawStream));

    std::shared_ptr<Camera> camera = Camera::create(std::move(data), cameraName, streams);

    registerCamera(std::move(camera));
    LOG(IPU7, Info) << "Registered Camera[" << numCameras << "] \"" << cameraName;

    numCameras++;
    return numCameras ? 0 : -ENODEV;
}

REGISTER_PIPELINE_HANDLER(PipelineHandlerIPU7)

} /* namespace libcamera */
