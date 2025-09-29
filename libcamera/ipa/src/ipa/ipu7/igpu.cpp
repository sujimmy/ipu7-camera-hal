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
#include <cstdint>
#include <utility>
#include <vector>

#include <libcamera/base/file.h>
#include <libcamera/base/log.h>
#include <libcamera/base/utils.h>

#include <libcamera/control_ids.h>
#include <libcamera/framebuffer.h>
#include <libcamera/ipa/ipa_interface.h>
#include <libcamera/ipa/ipa_module_info.h>
#include <libcamera/ipa/ipu7_igpu_ipa_interface.h>

#include "libcamera/internal/mapped_framebuffer.h"
#include "libcamera/internal/yaml_parser.h"

#include "IGPUIPAServer.h"
#include "IGPUHeader.h"

namespace libcamera {

LOG_DEFINE_CATEGORY(IGPUIPA)

namespace ipa::igpu {

/**
 * \brief The GPU IPA implementation
 *
 * The IPU7 Pipeline defines an IPU7-specific interface for communication
 * between the PipelineHandler and the IPA module.
 *
 * We extend the IPAGPUInterface to implement our algorithms and handle
 * calls from the GPU PipelineHandler to satisfy requests from the
 * application.
 *
 * The GPU has further processing blocks to support image quality
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
class IGPUIPA : public IPAGPUInterface, public Loggable {
public:
    IGPUIPA();
    ~IGPUIPA();

    int init(const std::string &libPath) override;
    int start() override { return 0; }
    void stop() override {}
protected:
    std::string logPrefix() const override;

private:
    std::unique_ptr<IGPUIPAServer> mIPAServer;
};

IGPUIPA::IGPUIPA() {
    mIPAServer = std::make_unique<IGPUIPAServer>();
}

IGPUIPA::~IGPUIPA() {
}

int IGPUIPA::init(const std::string &libPath) {
    LOG(IGPUIPA, Debug) << libPath;
    return mIPAServer->init(libPath);
}

std::string IGPUIPA::logPrefix() const {
    return "igpu";
}

} /* namespace ipa::igpu */

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
    IGPU_IPA_VERSION,
    "PipelineHandlerIPU7",
    "igpu",
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
    return new ipa::igpu::IGPUIPA();
}
}

} /* namespace libcamera */
