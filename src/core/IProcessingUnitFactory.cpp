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

#define LOG_TAG IProcessingUnitFactory

#include "src/core/IProcessingUnitFactory.h"

#include "PlatformData.h"
#include "iutils/CameraLog.h"

#include "ProcessingUnit.h"

#include "SwImageProcessor.h"

namespace icamera {

IProcessingUnit* IProcessingUnitFactory::createIProcessingUnit(int cameraId,
                                                               std::shared_ptr<CameraScheduler> scheduler) {
    if (PlatformData::isUsePSysProcessor(cameraId)) {
        LOG1("%s, Using IPU PSys to do image processing.", __func__);
        return new ProcessingUnit(cameraId, scheduler);
    }

    LOG1("%s, Using software to do color conversion.", __func__);
    return new SwImageProcessor(cameraId);
}
}  // namespace icamera
