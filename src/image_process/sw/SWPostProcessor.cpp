/*
 * Copyright (C) 2019-2022 Intel Corporation
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

#define LOG_TAG SWPostProcessor

#include "iutils/CameraLog.h"
#include "SWPostProcessor.h"
#include "ImageScalerCore.h"
#include "ImageConverter.h"
#include "PlatformData.h"

namespace icamera {

SWPostProcessor::SWPostProcessor()
{
    LOG2("enter %s", __func__);
}

SWPostProcessor::~SWPostProcessor()
{
    LOG2("enter %s", __func__);
}

std::unique_ptr<IImageProcessor> IImageProcessor::createImageProcessor()
{
    return std::unique_ptr<SWPostProcessor>(new SWPostProcessor());
}

//If support this kind of post process type in current OS
bool IImageProcessor::isProcessingTypeSupported(PostProcessType type)
{
    int supportedType = POST_PROCESS_CONVERT | POST_PROCESS_JPEG_ENCODING;
    if (PlatformData::useGPUProcessor())
        supportedType |= POST_PROCESS_GPU;
    else
        supportedType |= POST_PROCESS_SCALING;

    return supportedType & type;
}

// The frame crop is handled together with frame scaling
status_t SWPostProcessor::cropFrame(const std::shared_ptr<CameraBuffer> &input,
                                    std::shared_ptr<CameraBuffer> &output)
{
    UNUSED(input);
    UNUSED(output);
    return OK;
}

status_t SWPostProcessor::scaleFrame(const std::shared_ptr<CameraBuffer> &input,
                                     std::shared_ptr<CameraBuffer> &output)
{
    LOG2("%s: src: %dx%d,format 0x%x, dest: %dx%d format 0x%x",
         __func__, input->getWidth(), input->getHeight(), input->getFormat(),
         output->getWidth(), output->getHeight(), output->getFormat());

    ImageScalerCore::downScaleImage(input->getBufferAddr(), output->getBufferAddr(),
                                    output->getWidth(), output->getHeight(), output->getStride(),
                                    input->getWidth(), input->getHeight(), input->getStride(),
                                    input->getFormat());

    return OK;
}

// Software image processor doesn't support rotate
status_t SWPostProcessor::rotateFrame(const std::shared_ptr<CameraBuffer> &input,
                                      std::shared_ptr<CameraBuffer> &output,
                                      int angle, std::vector<uint8_t> &rotateBuf)
{
    UNUSED(input);
    UNUSED(output);
    return OK;
}

status_t SWPostProcessor::convertFrame(const std::shared_ptr<CameraBuffer> &input,
                                       std::shared_ptr<CameraBuffer> &output)
{
    LOG2("%s: src: %dx%d,format 0x%x, dest: %dx%d format 0x%x",
         __func__, input->getWidth(), input->getHeight(), input->getFormat(),
         output->getWidth(), output->getHeight(), output->getFormat());

    switch (output->v4l2Fmt()) {
        case V4L2_PIX_FMT_YVU420:
            // XXX -> YV12
            ImageConverter::convertBuftoYV12(input->getFormat(), input->getWidth(),
                                             input->getHeight(), input->getStride(),
                                             output->getStride(), input->getBufferAddr(),
                                             output->getBufferAddr());
            break;
        case V4L2_PIX_FMT_NV21:
            // XXX -> NV21
            ImageConverter::convertBuftoNV21(input->getFormat(), input->getWidth(),
                                             input->getHeight(), input->getStride(),
                                             output->getStride(), input->getBufferAddr(),
                                             output->getBufferAddr());
            break;
        case V4L2_PIX_FMT_YUYV:
            // XXX -> YUYV
            ImageConverter::convertBuftoYUYV(input->getFormat(), input->getWidth(),
                                             input->getHeight(), input->getStride(),
                                             output->getStride(), input->getBufferAddr(),
                                             output->getBufferAddr());
            break;
        default:
            LOGE("%s: not implement for color conversion 0x%x -> 0x%x!",
                 __func__, input->getFormat(), output->getFormat());
            return UNKNOWN_ERROR;
    }

    return OK;
}
} /* namespace icamera */
