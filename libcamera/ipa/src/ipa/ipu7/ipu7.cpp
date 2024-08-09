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

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <stdint.h>
#include <utility>
#include <vector>

#include <libcamera/base/file.h>
#include <libcamera/base/log.h>
#include <libcamera/base/utils.h>

#include <libcamera/control_ids.h>
#include <libcamera/framebuffer.h>
#include <libcamera/ipa/ipa_interface.h>
#include <libcamera/ipa/ipa_module_info.h>
#include <libcamera/ipa/ipu7_ipa_interface.h>

#include "libcamera/internal/mapped_framebuffer.h"
#include "libcamera/internal/yaml_parser.h"

#include "IPAServer.h"

namespace libcamera {

LOG_DEFINE_CATEGORY(IPAIPU)

namespace ipa::ipu7 {

/**
 * \brief The IPU IPA implementation
 *
 * The IPU Pipeline defines an IPU-specific interface for communication
 * between the PipelineHandler and the IPA module.
 *
 * We extend the IPAIPUInterface to implement our algorithms and handle
 * calls from the IPU PipelineHandler to satisfy requests from the
 * application.
 *
 * The IPU has further processing blocks to support image quality
 * improvements through bayer and temporal noise reductions, however those are
 * not supported in the current implementation, and will use default settings as
 * provided by the kernel driver.
 *
 * Demosaicing is operating with the default parameters and could be further
 * optimised to provide improved sharpening coefficients, checker artifact
 * removal, and false color correction.
 *
 * Additional image enhancements can be made by providing lens and
 * sensor-specific tuning to adapt for Black Level compensation (BLC), Lens
 * shading correction (SHD) and Color correction (CCM).
 */
class IPAIPU : public IPAIPU7Interface, public Loggable, public IIPAIPUCallback {
public:
    IPAIPU();
    ~IPAIPU();

    int init(const uint32_t bufferId) override;

    int start() override { return 0; }
    void stop() override {}

    int requestSync(const IPACmdInfo& cmdInfo) override;
    void requestASync(const IPACmdInfo& cmdInfo) override;

    void mapBuffers(const std::vector<IPABuffer> &buffers) override;
    void unmapBuffers(const std::vector<unsigned int> &ids) override;

    void returnRequestReady(int cameraId, int tuningMode, uint32_t cmd, int ret) override;
    void* getBuffer(uint32_t bufferId) override;

protected:
    std::string logPrefix() const override;

private:
    std::map<unsigned int, MappedFrameBuffer> buffers_;
    std::unique_ptr<IPAServer> mIPAServer;
};

IPAIPU::IPAIPU() {
    mIPAServer = std::make_unique<IPAServer>(this);
}

IPAIPU::~IPAIPU() {
}

void IPAIPU::returnRequestReady(int cameraId, int tuningMode, uint32_t cmd, int ret) {
    IPACmdInfo cmdInfo = { cameraId, tuningMode, cmd, 0 };

    requestReady.emit(cmdInfo, ret);
}

void* IPAIPU::getBuffer(uint32_t bufferId) {
    auto it = buffers_.find(bufferId);
    if (it == buffers_.end()) {
        LOG(IPAIPU, Error) << " buffer id " << bufferId << " isn't found";
        return nullptr;
    }

    Span<uint8_t> mem = it->second.planes()[0];

    return mem.data();
}

int IPAIPU::init(const uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " init bufferId " << bufferId;

    auto it = buffers_.find(bufferId);
    if (it == buffers_.end()) {
        LOG(IPAIPU, Error) << " init " << bufferId << " no found";
        return -1;
    }

    Span<uint8_t> mem = it->second.planes()[0];

    return mIPAServer->init(mem.data());
}

std::string IPAIPU::logPrefix() const {
    return "ipu7";
}

int IPAIPU::requestSync(const IPACmdInfo& cmdInfo) {
    LOG(IPAIPU, Debug) << "IPAIPU::requestSync cameraId " << cmdInfo.cameraId << " tuningMode "
                       << cmdInfo.tuningMode << " cmd " << cmdInfo.cmd << " bufferId "
                       << cmdInfo.bufferId;

    return 0;
}

void IPAIPU::requestASync(const IPACmdInfo& cmdInfo) {
    LOG(IPAIPU, Debug) << "IPAIPU::requestASync cameraId " << cmdInfo.cameraId << " tuningMode "
                       << cmdInfo.tuningMode << " cmd " << cmdInfo.cmd << " bufferId "
                       << cmdInfo.bufferId;

    auto it = buffers_.find(cmdInfo.bufferId);
    if (it == buffers_.end()) {
        LOG(IPAIPU, Error) << " buffer id " << cmdInfo.bufferId << " isn't found";
        return;
    }

    Span<uint8_t> mem = it->second.planes()[0];

    mIPAServer->sendRequest(cmdInfo.cameraId, cmdInfo.tuningMode, cmdInfo.cmd, mem);
}

/**
 * \brief Map the parameters and stats buffers allocated in the pipeline handler
 * \param[in] buffers The buffers to map
 */
void IPAIPU::mapBuffers(const std::vector<IPABuffer> &buffers)
{
    for (const IPABuffer &buffer : buffers) {
        const FrameBuffer fb(buffer.planes);
        LOG(IPAIPU, Debug) << " map buffer.id " << buffer.id;
        buffers_.emplace(buffer.id,
            MappedFrameBuffer(&fb, MappedFrameBuffer::MapFlag::ReadWrite));
    }
}

/**
 * \brief Unmap the parameters and stats buffers
 * \param[in] ids The IDs of the buffers to unmap
 */
void IPAIPU::unmapBuffers(const std::vector<unsigned int> &ids)
{
    for (unsigned int id : ids) {
        auto it = buffers_.find(id);
        if (it == buffers_.end())
            continue;

        LOG(IPAIPU, Debug) << " unmap buffer.id " << id;
        buffers_.erase(it);
    }
}

} /* namespace ipa::ipu7 */

/**
 * \brief External IPA module interface
 *
 * The IPAModuleInfo is required to match an IPA module construction against the
 * intented pipeline handler with the module. The API and pipeline handler
 * versions must match the corresponding IPA interface and pipeline handler.
 *
 * \sa struct IPAModuleInfo
 */
extern "C" {
const struct IPAModuleInfo ipaModuleInfo = {
    IPA_MODULE_API_VERSION,
    IPU7_IPA_VERSION,
    "PipelineHandlerIPU7",
    "ipu7",
};

/**
 * \brief Create an instance of the IPA interface
 *
 * This function is the entry point of the IPA module. It is called by the IPA
 * manager to create an instance of the IPA interface for each camera. When
 * matched against with a pipeline handler, the IPAManager will construct an IPA
 * instance for each associated Camera.
 */
IPAInterface *ipaCreate()
{
    return new ipa::ipu7::IPAIPU();
}
}

} /* namespace libcamera */
