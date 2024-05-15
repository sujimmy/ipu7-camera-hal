#
#  Copyright (C) 2017-2024 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

set(CORE_SRCS
    ${CORE_DIR}/BufferQueue.cpp
    ${CORE_DIR}/CameraBuffer.cpp
    ${CORE_DIR}/CameraEvent.cpp
    ${CORE_DIR}/LensHw.cpp
    ${CORE_DIR}/SensorHwCtrl.cpp
    ${CORE_DIR}/SofSource.cpp
    CACHE INTERNAL "core sources"
    )

if (IPU_VER MATCHES "ipu7")
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/CameraBufferPool.cpp
        ${CORE_DIR}/CameraDevice.cpp
        ${CORE_DIR}/CameraContext.cpp
        ${CORE_DIR}/CameraStream.cpp
        ${CORE_DIR}/CaptureUnit.cpp
        ${CORE_DIR}/DeviceBase.cpp
        ${CORE_DIR}/IProcessingUnitFactory.cpp
        ${CORE_DIR}/ProcessingUnit.cpp
        ${CORE_DIR}/RequestThread.cpp
        ${CORE_DIR}/PSysDevice.cpp
        CACHE INTERNAL "core sources"
        )
    if (CAL_BUILD)
        set(CORE_SRCS
            ${CORE_SRCS}
            ${CORE_DIR}/SwPostProcessUnit.cpp
            CACHE INTERNAL "core sources"
            )
    endif() #CAL_BUILD
else ()
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/CaptureUnit.cpp
        ${CORE_DIR}/DeviceBase.cpp
        ${CORE_DIR}/CameraStream.cpp
        ${CORE_DIR}/CameraDevice.cpp
        ${CORE_DIR}/IProcessingUnitFactory.cpp
        ${CORE_DIR}/RequestThread.cpp
        CACHE INTERNAL "core sources"
        )
endif ()

# FRAME_SYNC_S
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/SyncManager.cpp
        CACHE INTERNAL "core sources"
        )
# FRAME_SYNC_E

# CSI_META_S
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/CsiMetaDevice.cpp
        CACHE INTERNAL "core sources"
        )
# CSI_META_E

# FILE_SOURCE_S
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/FileSource.cpp
        CACHE INTERNAL "core sources"
        )
# FILE_SOURCE_E

if (NOT CAL_BUILD)
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/SwImageProcessor.cpp
        CACHE INTERNAL "core sources"
        )
endif()
    set(CORE_SRCS
        ${CORE_SRCS}
        CACHE INTERNAL "core sources"
        )

# IPU_PROCESSING_S
    set(CORE_SRCS
        ${CORE_SRCS}
        ${CORE_DIR}/ProcessingUnit.cpp
        ${CORE_DIR}/processingUnit/IPipeManagerFactory.cpp
        CACHE INTERNAL "core sources"
       )
    if (IPU_VER MATCHES "ipu7")
        set(CORE_SRCS
            ${CORE_SRCS}
            ${CORE_DIR}/IpuPacAdaptor.cpp
            ${CORE_DIR}/processingUnit/PipeManager.cpp
            ${CORE_DIR}/processingUnit/PipeLine.cpp
            ${CORE_DIR}/processingUnit/CBStage.cpp
            ${CORE_DIR}/processingUnit/ShareReferBufferPool.cpp
            CACHE INTERNAL "core sources"
        )
    endif()

    if (USE_PG_LITE_PIPE)
        set(CORE_SRCS
            ${CORE_SRCS}
            ${CORE_DIR}/processingUnit/ShareReferBufferPool.cpp
            CACHE INTERNAL "core sources"
        )
    endif()
# IPU_PROCESSING_E

