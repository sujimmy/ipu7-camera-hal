/*
 * Copyright (C) 2022-2025 Intel Corporation.
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

#ifndef I_PIPE_MANAGER_FACTORY
#define I_PIPE_MANAGER_FACTORY

#include "IPipeManager.h"
#include "CameraScheduler.h"

namespace icamera {

/*
 * \factory class IPipeManagerFactory
 * This class is used to create the right instance of IPipeManager
 */
class IPipeManagerFactory {
 public:
    /**
     * \brief Select the IPipeManager accroding to config
     *
     * \param cameraId: the camera id
     * \param PipeManagerCallback: the callback object
     *
     * \return the IPipeManager class
     */
    static IPipeManager* createIPipeManager(int cameraId, PipeManagerCallback* callback,
                                            std::shared_ptr<CameraScheduler>& scheduler);
};
}  // namespace icamera

#endif // I_PIPE_MANAGER_FACTORY
