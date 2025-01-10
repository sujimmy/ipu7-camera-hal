/*
* INTEL CONFIDENTIAL
* Copyright (c) 2022 Intel Corporation
* All Rights Reserved.
*
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Intel Corporation or its
* suppliers or licensors. Title to the Material remains with Intel
* Corporation or its suppliers and licensors. The Material may contain trade
* secrets and proprietary and confidential information of Intel Corporation
* and its suppliers and licensors, and is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may be
* used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*
* Unless otherwise agreed by Intel in writing, you may not remove or alter
* this notice or any other notice embedded in Materials by Intel or Intels
* suppliers or licensors in any way.
*/

#include "GraphResolutionConfiguratorInclude.h"
#include "GraphResolutionConfigurator.h"
#include <algorithm>
#include <math.h>

/*
 * External Interfaces
 */

 GraphResolutionConfigurator::GraphResolutionConfigurator(IStaticGraphConfig* staticGraph)
{
    _staticGraph = staticGraph;

    if (_staticGraph == nullptr)
    {
        return;
    }

    if (initRunKernelCoord(GraphResolutionConfiguratorKernelRole::DownScaler, _downscalerRunKernelCoord) != StaticGraphStatus::SG_OK ||
        initRunKernelCoord(GraphResolutionConfiguratorKernelRole::UpScaler, _upscalerRunKernelCoord) != StaticGraphStatus::SG_OK ||
        initRunKernelCoord(GraphResolutionConfiguratorKernelRole::FinalCropper, _cropperRunKernelCoord) != StaticGraphStatus::SG_OK ||
        initOutputRunKernelCoord(_outputRunKernelCoord) != StaticGraphStatus::SG_OK ||
        initKernelCoordsForUpdate() != StaticGraphStatus::SG_OK)
    {
        _staticGraph = nullptr;
        return;
    }

    auto outputRunKernel = getRunKernel(_outputRunKernelCoord);
    auto cropperRunKernel = getRunKernel(_cropperRunKernelCoord);

    _originalCropOfFinalCropper = cropperRunKernel->resolution_info->input_crop;

    // Calculate total scaling between sensor and output
    // We want to calculate the scaling ratio without taking any cropping into consideration
    _widthIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_width -
        outputRunKernel->resolution_history->input_crop.left -
        outputRunKernel->resolution_history->input_crop.right) / outputRunKernel->resolution_history->output_width;

    _heightIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_height -
        outputRunKernel->resolution_history->input_crop.top -
        outputRunKernel->resolution_history->input_crop.bottom) / outputRunKernel->resolution_history->output_height;

    _originalCropInputToScaler = cropperRunKernel->resolution_history->input_crop;

    _originalCropScalerToOutput.left = outputRunKernel->resolution_history->input_crop.left - _originalCropInputToScaler.left;
    _originalCropScalerToOutput.right = outputRunKernel->resolution_history->input_crop.right - _originalCropInputToScaler.right;
    _originalCropScalerToOutput.top = outputRunKernel->resolution_history->input_crop.top - _originalCropInputToScaler.top;
    _originalCropScalerToOutput.bottom = outputRunKernel->resolution_history->input_crop.bottom - _originalCropInputToScaler.bottom;

    // Now take into account the scaling performed by this output
    // (Output kernel may perform scaling and cropping when graph contains post processing)
    if (outputRunKernel->resolution_info != nullptr)
    {
        // First add the crop, translated to sensor units, w/out this kernel's scaling since input crop is done before scaling.
        _originalCropScalerToOutput.left += static_cast<int32_t>(outputRunKernel->resolution_info->input_crop.left * _widthIn2OutScale);
        _originalCropScalerToOutput.right += static_cast<int32_t>(outputRunKernel->resolution_info->input_crop.right * _widthIn2OutScale);
        _originalCropScalerToOutput.top += static_cast<int32_t>(outputRunKernel->resolution_info->input_crop.top * _heightIn2OutScale);
        _originalCropScalerToOutput.bottom += static_cast<int32_t>(outputRunKernel->resolution_info->input_crop.bottom * _heightIn2OutScale);

        _widthIn2OutScale *= static_cast<double>(outputRunKernel->resolution_info->input_width -
            outputRunKernel->resolution_info->input_crop.left -
            outputRunKernel->resolution_info->input_crop.right) / outputRunKernel->resolution_info->output_width;

        _heightIn2OutScale *= static_cast<double>(outputRunKernel->resolution_info->input_height -
            outputRunKernel->resolution_info->input_crop.top -
            outputRunKernel->resolution_info->input_crop.bottom) / outputRunKernel->resolution_info->output_height;
    }

     // Remove sensor binning from In2Out total ratios and saved cropping values
    SensorMode* sensorMode = nullptr;
    _staticGraph->getSensorMode(&sensorMode);
    if (sensorMode == nullptr)
    {
        _staticGraph = nullptr;
        return;
    }

    if (sensorMode->horizontalScalingDenominator) {
        _sensorHorizontalScaling = static_cast<double>(sensorMode->horizontalScalingNumerator) / sensorMode->horizontalScalingDenominator;
    }
    if (sensorMode->verticalScalingDenominator) {
        _sensorVerticalScaling = static_cast<double>(sensorMode->verticalScalingNumerator) / sensorMode->verticalScalingDenominator;
    }

    _widthIn2OutScale = _widthIn2OutScale * _sensorHorizontalScaling;
    _heightIn2OutScale = _heightIn2OutScale * _sensorVerticalScaling;

    _sensorHorizontalCropLeft = sensorMode->horizontalCropOffset;
    _sensorHorizontalCropRight = outputRunKernel->resolution_history->input_width -
        sensorMode->horizontalCropOffset - sensorMode->croppedImageWidth;
    _sensorVerticalCropTop = sensorMode->verticalCropOffset;
    _sensorVerticalCropBottom = outputRunKernel->resolution_history->input_height -
        sensorMode->verticalCropOffset - sensorMode->croppedImageHeight;

    // Input crop to scaler includes sensor, so we need to remove it
    _originalCropInputToScaler.top = static_cast<int32_t>((_originalCropInputToScaler.top - static_cast<int32_t>(_sensorVerticalCropTop)) * _sensorHorizontalScaling);
    _originalCropInputToScaler.bottom = static_cast<int32_t>((_originalCropInputToScaler.bottom - static_cast<int32_t>(_sensorVerticalCropBottom)) * _sensorHorizontalScaling);
    _originalCropInputToScaler.left = static_cast<int32_t>((_originalCropInputToScaler.left - static_cast<int32_t>(_sensorHorizontalCropLeft)) * _sensorVerticalScaling);
    _originalCropInputToScaler.right = static_cast<int32_t>((_originalCropInputToScaler.right - static_cast<int32_t>(_sensorHorizontalCropRight)) * _sensorVerticalScaling);
 }

/*
 * External Interfaces
 */

StaticGraphStatus GraphResolutionConfigurator::getZoomKeyResolutionIndex(ZoomKeyResolutions* zoomKeyResolutions, SensorRoi sensorRoi, uint32_t& selectedIndex)
{
    uint32_t width = sensorRoi.width;
    uint32_t height = sensorRoi.height;

     // SelectedIndex 0 means use full sensor
     // SelectedIndex n+1 means use key resolution #n
    selectedIndex = zoomKeyResolutions->numberOfZoomKeyOptions;
    if (width > 0 && height > 0)
    {
        for (uint32_t i = 0; i < zoomKeyResolutions->numberOfZoomKeyOptions; ++i)
        {
            if (width > zoomKeyResolutions->zoomKeyResolutionOptions[i].width || height > zoomKeyResolutions->zoomKeyResolutionOptions[i].height)
            {
                // This key resolution is too small, so use previous one (selected index i means use previous key resolution, not this one)
                selectedIndex = i;
                break;
            }
        }
    }

    return StaticGraphStatus::SG_OK;
}

// This function receives a static graph and updates kernels resolution info and resolution history to
// perform the required crop and scaling for the give roi
// Expected changes in graph:
// Down scaler resolution info - crop & scale
// Up scaler resolution info - crop  & scale
//
// Up scaler resolution history
// All kerenls after upscaler - resolution history
//
// When using key resolutions:
// If previous ROI is supplied, the function will also return indication if key resolution has changed.
// A change of key resolution indicates that resolutions of entire pipe had changed until after the up scaler
StaticGraphStatus GraphResolutionConfigurator::updateStaticGraphConfig(const RegionOfInterest& roi, const RegionOfInterest& prevRoi,
    bool isCenteredZoom, bool prevIsCenteredZoom, bool& isKeyResolutionChanged)
{
#if SUPPORT_KEY_RESOLUTIONS == 1
    if (_staticGraph == nullptr)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    //
    // Step #1 Update according to this ROI's key resolution
    //
    // Get ROI in sensor dimensions

    // If zoom is centered, calculate the pan & tilt
    RegionOfInterest userRoi = roi;
    RegionOfInterest prevUserRoi = prevRoi;

    if (isCenteredZoom == true)
    {
        userRoi.panFactor = (1 - userRoi.zoomFactor) / 2;
        userRoi.tiltFactor = (1 - userRoi.zoomFactor) / 2;
    }

    if (prevIsCenteredZoom == true)
    {
        prevUserRoi.panFactor = (1 - prevUserRoi.zoomFactor) / 2;
        prevUserRoi.tiltFactor = (1 - prevUserRoi.zoomFactor) / 2;
    }

    SensorRoi sensorRoi;
    if (getSensorRoi(userRoi, sensorRoi) != StaticGraphStatus::SG_OK)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Key resolution index
    uint32_t keyResIndex = 0;
    if (isCenteredZoom == true)
    {
        ZoomKeyResolutions* zoomKeyResolutions = nullptr;
        if (_staticGraph->getZoomKeyResolutions(&zoomKeyResolutions) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }

        // Get key resolution for this ROI
        // Special case for factor 1, this means there is no zoom, so we select index 0
        if (userRoi.zoomFactor == 1.0)
        {
            keyResIndex = 0;
        }
        else if (getZoomKeyResolutionIndex(zoomKeyResolutions, sensorRoi, keyResIndex) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }

        // Update the static configuration according to the key resolution index
        // Copy the original kernels configuration a _kernels
        if (_staticGraph->updateConfiguration(keyResIndex) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }
    }
    else
    {
        if (_staticGraph->updateConfiguration() != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }
    }

     // Get key resolution for previous ROI
    uint32_t prevKeyResIndex = 0;

    if (prevIsCenteredZoom == true)
    {
        ZoomKeyResolutions* zoomKeyResolutions = nullptr;
        if (_staticGraph->getZoomKeyResolutions(&zoomKeyResolutions) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }

        SensorRoi prevSensorRoi;
        if (getSensorRoi(prevUserRoi, prevSensorRoi) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }

        // Special case for factor 1, this means there is no zoom, so we select index 0
        if (prevUserRoi.zoomFactor == 1.0)
        {
            prevKeyResIndex = 0;
        }
        else if (getZoomKeyResolutionIndex(zoomKeyResolutions, prevSensorRoi, prevKeyResIndex) != StaticGraphStatus::SG_OK)
        {
            return StaticGraphStatus::SG_ERROR;
        }
    }
    // Update whether if key resolution has changed
    isKeyResolutionChanged = (keyResIndex == prevKeyResIndex) ? false : true;

    //
    // Step #2 Dynamic update according to this ROI
    //
    return updateRunKernelOfScalers(sensorRoi);
#else
    // This version is not supported when key resolutions are not used (NVL and up)
    // Will be removed once driver will use new API for Ipu8GraphResolutionConfigurator
    (void*)&roi;
    (void*)&prevRoi;
    (void*)&isCenteredZoom;
    (void*)&prevIsCenteredZoom;
    (void*)&isKeyResolutionChanged;

     return StaticGraphStatus::SG_ERROR;
#endif

}

// This function translates ROI from factors (as given by user) to sensor resolution (as required by resolution Configurator)
// There are 2 modes of work -
// if userRoi.fromInput is true it means zoomFactor panFactor and tiltFactor are relative to sensor FOV
// if userRoi.fromInput is false it means zoomFactor panFactor and tiltFactor are relative to preview pin output FOV
StaticGraphStatus GraphResolutionConfigurator::getSensorRoi(const RegionOfInterest& userRoi, SensorRoi& sensorRoi)
{
    if (_staticGraph == nullptr)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    auto outputRunKernel = getRunKernel(_outputRunKernelCoord);

    if (userRoi.fromInput == true)
    {
        // Not supported
        return StaticGraphStatus::SG_ERROR;
    }

    // Calculate ROI on output, using original output since user ROI is relative to full output ROI (not after some zoom was performed)
    StaticGraphKernelRes* outputRunKernelResolution = outputRunKernel->resolution_info;
    if (outputRunKernelResolution == nullptr)
    {
        // Pipe output is not RCB (post processing kernel)
        outputRunKernelResolution = outputRunKernel->resolution_history;
    }

    uint32_t outputLeft = static_cast<uint32_t>(outputRunKernelResolution->output_width * userRoi.panFactor);
    uint32_t outputRight = static_cast<uint32_t>(outputRunKernelResolution->output_width * (1 - userRoi.panFactor - userRoi.zoomFactor));
    uint32_t outputTop = static_cast<uint32_t>(outputRunKernelResolution->output_height * userRoi.tiltFactor);
    uint32_t outputBottom = static_cast<uint32_t>(outputRunKernelResolution->output_height * (1 - userRoi.tiltFactor - userRoi.zoomFactor));

    uint32_t outputWidth = outputRunKernelResolution->output_width - outputLeft - outputRight;
    uint32_t outputHeight = outputRunKernelResolution->output_height - outputTop - outputBottom;

    // Total input to pipe (after sensor cropping and scaling)
    uint32_t inputWidth = static_cast<uint32_t>((outputRunKernel->resolution_history->input_width - _sensorHorizontalCropLeft -_sensorHorizontalCropRight) * _sensorHorizontalScaling);
    uint32_t inputHeight = static_cast<uint32_t>((outputRunKernel->resolution_history->input_height - _sensorVerticalCropTop - _sensorVerticalCropBottom) * _sensorVerticalScaling);

    // Translate to ROI on input
    // We round down to make sure crops are not less than original crop
    // We add to sensor ROI the cropping done after scalers since it will be cropped in zoom configurations as well.
    sensorRoi.width = GRA_ROUND_DOWN(static_cast<uint32_t>(outputWidth * _widthIn2OutScale + _originalCropScalerToOutput.left + _originalCropScalerToOutput.right), 2);
    sensorRoi.height = GRA_ROUND_DOWN(static_cast<uint32_t>(outputHeight * _heightIn2OutScale + _originalCropScalerToOutput.top + _originalCropScalerToOutput.bottom), 2);
    sensorRoi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>((outputLeft * _widthIn2OutScale) + _originalCropInputToScaler.left), 2);
    sensorRoi.cropRight = inputWidth - sensorRoi.width - sensorRoi.cropLeft;
    sensorRoi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>((outputTop * _heightIn2OutScale) + _originalCropInputToScaler.top), 2);
    sensorRoi.cropBottom = inputHeight - sensorRoi.height - sensorRoi.cropTop;

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelOfScalers(SensorRoi& roi)
{
    StaticGraphStatus ret = StaticGraphStatus::SG_OK;

    auto downscalerRunKernel = getRunKernel(_downscalerRunKernelCoord);
    auto upscalerRunKernel = getRunKernel(_upscalerRunKernelCoord);
    auto cropperRunKernel = getRunKernel(_cropperRunKernelCoord);

    uint32_t inputWidth = downscalerRunKernel->resolution_info->input_width;
    uint32_t inputHeight = downscalerRunKernel->resolution_info->input_height;

    uint32_t outputWidth = cropperRunKernel->resolution_info->output_width;
    uint32_t outputHeight = cropperRunKernel->resolution_info->output_height;

    StaticGraphKernelResCrop downscalerCropHistory;
    downscalerCropHistory.left = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.left - static_cast<int32_t>(_sensorHorizontalCropLeft)) * _sensorHorizontalScaling);
    downscalerCropHistory.right = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.right - static_cast<int32_t>(_sensorHorizontalCropRight)) * _sensorHorizontalScaling);
    downscalerCropHistory.top = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.top - static_cast<int32_t>(_sensorVerticalCropTop)) * _sensorVerticalScaling);
    downscalerCropHistory.bottom = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.bottom - static_cast<int32_t>(_sensorVerticalCropBottom)) * _sensorVerticalScaling);

    // If ROI is larger than scaler's output resolution - we downscale
    if (roi.width >= outputWidth)
    {
        // Only down scaler is active
        if (updateRunKernelDownScaler(downscalerRunKernel, roi, inputWidth, inputHeight,
            outputWidth, outputHeight, &downscalerCropHistory) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }

        updateRunKernelPassThrough(upscalerRunKernel, outputWidth, outputHeight);

        // When downscaling, cropper is not a part of dynamic scaling, even if it was a part of static configuration.
        updateRunKernelPassThrough(cropperRunKernel, outputWidth, outputHeight);
    }
    else
    {
        // Configure downscaler and upscaler according to upscaler constraints

        // Update upscaler info, according constraints. Returns the expected input width and height for upscaler.
        uint32_t upscalerActualInputWidth;
        uint32_t upscalerActualInputHeight;
        uint32_t upscalerActualOutputWidth;
        uint32_t upscalerActualOutputHeight;
        if (updateRunKernelUpScaler(upscalerRunKernel, roi.width, roi.height, outputWidth, outputHeight,
            upscalerActualInputWidth, upscalerActualInputHeight,
            upscalerActualOutputWidth, upscalerActualOutputHeight) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }

        // Update DS cropping and downscale according to the resolution the upscaler requires.
        if (updateRunKernelDownScaler(downscalerRunKernel, roi, inputWidth, inputHeight,
            upscalerActualInputWidth, upscalerActualInputHeight, &downscalerCropHistory) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }

        // Now that we're done, if downscaler is not doing any scaling, it is better to let it be bypassed and move the cropping to upscaler
        updateCroppingScaler(downscalerRunKernel, upscalerRunKernel);

        // Update ESPA crop if required
        updateRunKernelFinalCropper(cropperRunKernel, upscalerActualOutputWidth, upscalerActualOutputHeight, outputWidth, outputHeight);
    }

    // Update resolution histories according to decisions made above
    if (updateRunKernelResolutionHistory(upscalerRunKernel, downscalerRunKernel) != StaticGraphStatus::SG_OK)
    {
        ret = StaticGraphStatus::SG_ERROR;
    }

    if (updateRunKernelResolutionHistory(cropperRunKernel, upscalerRunKernel) != StaticGraphStatus::SG_OK)
    {
        ret = StaticGraphStatus::SG_ERROR;
    }

    // Update resolution history for relevant kernels
    for (auto& runKernelForUpdate : _kernelsForUpdate)
    {
        // We update all histories according to upscaler... ignoring any cropping from now on, even if we configured ESPA cropper.
        StaticGraphRunKernel* runKernelPtr = getRunKernel(runKernelForUpdate);
        if (updateRunKernelResolutionHistory(runKernelPtr, upscalerRunKernel, false) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }
    }

    return ret;
}

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelDownScaler(StaticGraphRunKernel* runKernel, SensorRoi& roi,
    uint32_t inputWidth, uint32_t inputHeight,
    uint32_t outputWidth, uint32_t outputHeight,
    StaticGraphKernelResCrop* originalScalerCrop)
{
    //
    // Configure scaler
    //
    runKernel->resolution_info->input_width = inputWidth;
    runKernel->resolution_info->input_height = inputHeight;

    runKernel->resolution_info->output_width = outputWidth;
    runKernel->resolution_info->output_height = outputHeight;

    // Take into consideration original crop from downscaler's resolution history
    // We assume that originally DS was configured to crop all the padding in its resolution history.
    // Otherwise - we will need to save original DS crop (but for each key resolution...)
    // roi crops were rounded, while original crop may still be odd numbers. We need to ignore 1 pixel diffs
    StaticGraphKernelResCrop* runKernelCrop = &runKernel->resolution_info->input_crop;
    runKernelCrop->left = static_cast<int32_t>(roi.cropLeft) - originalScalerCrop->left;
    runKernelCrop->right = static_cast<int32_t>(roi.cropRight) - originalScalerCrop->right;
    runKernelCrop->top = static_cast<int32_t>(roi.cropTop) - originalScalerCrop->top;
    runKernelCrop->bottom = static_cast<int32_t>(roi.cropBottom) - originalScalerCrop->bottom;

    // If we are very close to key resolution sizes, and sensor BYR order is not GRBG, we may have small negative crops here
    if (runKernelCrop->left < 0)
    {
        runKernelCrop->left = 0;
    }
    if (runKernelCrop->right < 0)
    {
        runKernelCrop->right = 0;
    }
    if (runKernelCrop->top < 0)
    {
        runKernelCrop->top = 0;
    }
    if (runKernelCrop->bottom < 0)
    {
        runKernelCrop->bottom = 0;
    }

    if (runKernelCrop->left & 1)
    {
        runKernelCrop->left = runKernelCrop->left - 1;
    }
    if (runKernelCrop->right & 1)
    {
        runKernelCrop->right = runKernelCrop->right - 1;
    }
    if (runKernelCrop->top & 1)
    {
        runKernelCrop->top = runKernelCrop->top - 1;
    }
    if (runKernelCrop->bottom & 1)
    {
        runKernelCrop->bottom = runKernelCrop->bottom - 1;
    }

    // In case ROI is too small for desired output resolution, we increase ROI
    if (adjustDownscalerCrop(runKernel->resolution_info) != StaticGraphStatus::SG_OK)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::adjustDownscalerCrop(StaticGraphKernelRes* scalerResInfo)
{
    if (scalerResInfo->input_width - scalerResInfo->input_crop.left - scalerResInfo->input_crop.right <
        scalerResInfo->output_width)
    {
        uint32_t extraPixels = scalerResInfo->output_width -
            (scalerResInfo->input_width - scalerResInfo->input_crop.left - scalerResInfo->input_crop.right);

        extraPixels = GRA_ROUND_UP(extraPixels, 4);
        uint32_t neededCrop = extraPixels / 2;

        scalerResInfo->input_crop.left -= neededCrop;
        if (scalerResInfo->input_crop.left < 0)
        {
            neededCrop += -scalerResInfo->input_crop.left;
            scalerResInfo->input_crop.left = 0;
        }

        scalerResInfo->input_crop.right -= neededCrop;
        if (scalerResInfo->input_crop.right < 0)
        {
            scalerResInfo->input_crop.right = 0;
            return StaticGraphStatus::SG_ERROR;
        }
    }

    if (scalerResInfo->input_height - scalerResInfo->input_crop.top - scalerResInfo->input_crop.bottom <
        scalerResInfo->output_height)
    {
        uint32_t extraPixels = scalerResInfo->output_height -
            (scalerResInfo->input_height - scalerResInfo->input_crop.top - scalerResInfo->input_crop.bottom);

        extraPixels = GRA_ROUND_UP(extraPixels, 4);
        uint32_t neededCrop = extraPixels / 2;

        scalerResInfo->input_crop.top -= neededCrop;
        if (scalerResInfo->input_crop.top < 0)
        {
            neededCrop += -scalerResInfo->input_crop.top;
            scalerResInfo->input_crop.top = 0;
        }

        scalerResInfo->input_crop.bottom -= neededCrop;
        if (scalerResInfo->input_crop.bottom < 0)
        {
            scalerResInfo->input_crop.bottom = 0;
            return StaticGraphStatus::SG_ERROR;
        }
    }

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelUpScaler(StaticGraphRunKernel* runKernel,
    uint32_t inputWidth, uint32_t inputHeight,
    uint32_t outputWidth, uint32_t outputHeight,
    uint32_t& upscalerActualInputWidth, uint32_t& upscalerActualInputHeight,
    uint32_t& upscalerActualOutputWidth, uint32_t& upscalerActualOutputHeight)
{
    static const int SCALE_PREC = 16;
    const uint32_t ia_pal_isp_upscaler_1_0__scaling_ratio__min = 4096;

    const uint32_t max_upscaling = (1 << SCALE_PREC) / ia_pal_isp_upscaler_1_0__scaling_ratio__min;

    StaticGraphStatus ret = StaticGraphStatus::SG_OK;

    upscalerActualOutputWidth = outputWidth;
    upscalerActualOutputHeight = outputHeight;

     // Find valid output configurations
    uint32_t stepW1 = 1;
    uint32_t stepH1 = 1;

    for (stepH1 = 1; stepH1 < outputHeight / 2; stepH1++)
    {
        double horStep = static_cast<double>(stepH1) * outputWidth / 2 / outputHeight;
        if (floor((horStep)) == horStep)
        {
            stepW1 = static_cast<uint32_t>(horStep) * 2;
            break;
        }
    }

    // Now try to work with "sensor" resolution - take original ESPA crop's values
    // This is usually better when US output is not regular (and mp/dp cropping is used) and/or DS input is irregular (and ESPA is fixing A/R in original settings)
    // HSD 15016169206 and 15017041003 are 2 examples
    uint32_t newOutputWidth = outputWidth + _originalCropOfFinalCropper.left + _originalCropOfFinalCropper.right;
    uint32_t newOutputHeight = outputHeight + _originalCropOfFinalCropper.top + _originalCropOfFinalCropper.bottom;

    uint32_t stepW2 = 1;
    uint32_t stepH2 = 1;

    for (stepH2 = 1; stepH2 < newOutputHeight / 2; stepH2++)
    {
        double horStep = static_cast<double>(stepH2) * newOutputWidth / 2 / newOutputHeight;
        if (floor((horStep)) == horStep)
        {
            stepW2 = static_cast<uint32_t>(horStep) * 2;
            break;
        }
    }

     // Select which steps to take
    uint32_t stepW = stepW1;
    uint32_t stepH = stepH1;

    if (stepW2 < stepW1)
    {
        stepW = stepW2;
        stepH = stepH2;
        upscalerActualOutputWidth = newOutputWidth;
        upscalerActualOutputHeight = newOutputHeight;
    }

    // The input to the upscaler should be multiple of (stepW, stepH) and also even numbers
    if (stepW % 2 != 0 || stepH % 2 != 0)
    {
        stepW *= 2;
        stepH *= 2;
    }

    // Increase ROI to minimum possible ROI
    upscalerActualInputWidth = (inputWidth > stepW && inputHeight > stepH) ? inputWidth : stepW;

    // Make sure ROI is a multiple of (stepW, stepH)
    upscalerActualInputWidth = GRA_ROUND_DOWN(upscalerActualInputWidth, stepW);
    upscalerActualInputHeight = (upscalerActualInputWidth / stepW) * stepH;

    if ((upscalerActualOutputWidth / upscalerActualInputWidth) > max_upscaling)
    {
        // Perform the max possible up scaling, downscaler will adjust itself
        upscalerActualInputWidth = upscalerActualOutputWidth / max_upscaling;
        upscalerActualInputWidth = GRA_ROUND_UP(upscalerActualInputWidth, stepW);
        upscalerActualInputHeight = (upscalerActualInputWidth / stepW) * stepH;
    }

    //
    // Configure scaler
    //
    runKernel->resolution_info->input_width = upscalerActualInputWidth;
    runKernel->resolution_info->input_height = upscalerActualInputHeight;

    runKernel->resolution_info->output_width = upscalerActualOutputWidth;
    runKernel->resolution_info->output_height = upscalerActualOutputHeight;

    // Upscaler crop is always 0
    runKernel->resolution_info->input_crop.left = 0;
    runKernel->resolution_info->input_crop.right = 0;
    runKernel->resolution_info->input_crop.top = 0;
    runKernel->resolution_info->input_crop.bottom = 0;

    return ret;
}

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelPassThrough(StaticGraphRunKernel* runKernel, uint32_t width, uint32_t height)
{
    runKernel->resolution_info->input_width = width;
    runKernel->resolution_info->output_width = width;

    runKernel->resolution_info->input_height = height;
    runKernel->resolution_info->output_height = height;

    runKernel->resolution_info->input_crop.left = 0;
    runKernel->resolution_info->input_crop.right = 0;
    runKernel->resolution_info->input_crop.top = 0;
    runKernel->resolution_info->input_crop.bottom = 0;

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelFinalCropper(StaticGraphRunKernel* runKernel, uint32_t inputWidth, uint32_t inputHeight,
    uint32_t outputWidth, uint32_t outputHeight)
{
    runKernel->resolution_info->input_width = inputWidth;
    runKernel->resolution_info->input_height = inputHeight;

    runKernel->resolution_info->output_width = outputWidth;
    runKernel->resolution_info->output_height = outputHeight;

     // Crop on the right & bottom in order not to influence resolution history
    runKernel->resolution_info->input_crop.left = 0;
    runKernel->resolution_info->input_crop.right = inputWidth - outputWidth;
    runKernel->resolution_info->input_crop.top = 0;
    runKernel->resolution_info->input_crop.bottom = inputHeight - outputHeight;

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::updateCroppingScaler(StaticGraphRunKernel* downscalerRunKernel, StaticGraphRunKernel* upscalerRunKernel)
 {
     // Is DS performing DS?
    int32_t widthAfterCtop = downscalerRunKernel->resolution_info->input_width -
        downscalerRunKernel->resolution_info->input_crop.left - downscalerRunKernel->resolution_info->input_crop.right;
    int32_t heightAfterCtop = downscalerRunKernel->resolution_info->input_height -
        downscalerRunKernel->resolution_info->input_crop.top - downscalerRunKernel->resolution_info->input_crop.bottom;

     if (widthAfterCtop == downscalerRunKernel->resolution_info->output_width &&
         heightAfterCtop == downscalerRunKernel->resolution_info->output_height)
     {
         // Better move the cropping to US
         upscalerRunKernel->resolution_info->input_crop.left = downscalerRunKernel->resolution_info->input_crop.left;
         upscalerRunKernel->resolution_info->input_crop.right = downscalerRunKernel->resolution_info->input_crop.right;
         upscalerRunKernel->resolution_info->input_crop.top = downscalerRunKernel->resolution_info->input_crop.top;
         upscalerRunKernel->resolution_info->input_crop.bottom = downscalerRunKernel->resolution_info->input_crop.bottom;

         upscalerRunKernel->resolution_info->input_width = downscalerRunKernel->resolution_info->input_width;
         upscalerRunKernel->resolution_info->input_height = downscalerRunKernel->resolution_info->input_height;

         downscalerRunKernel->resolution_info->input_crop.left = 0;
         downscalerRunKernel->resolution_info->input_crop.right = 0;
         downscalerRunKernel->resolution_info->input_crop.top = 0;
         downscalerRunKernel->resolution_info->input_crop.bottom = 0;

         downscalerRunKernel->resolution_info->output_width = downscalerRunKernel->resolution_info->input_width;
         downscalerRunKernel->resolution_info->output_height = downscalerRunKernel->resolution_info->input_height;
     }

     return StaticGraphStatus::SG_OK;
 }

StaticGraphStatus GraphResolutionConfigurator::updateRunKernelResolutionHistory(StaticGraphRunKernel* runKernel, StaticGraphRunKernel* prevRunKernel, bool updateResolution)
{
    runKernel->resolution_history->input_crop.left = prevRunKernel->resolution_history->input_crop.left +
        static_cast<uint32_t>(prevRunKernel->resolution_info->input_crop.left / _sensorHorizontalScaling);
    runKernel->resolution_history->input_crop.right = prevRunKernel->resolution_history->input_crop.right +
        static_cast<uint32_t>(prevRunKernel->resolution_info->input_crop.right / _sensorHorizontalScaling);
    runKernel->resolution_history->input_crop.top = prevRunKernel->resolution_history->input_crop.top +
        static_cast<uint32_t>(prevRunKernel->resolution_info->input_crop.top / _sensorVerticalScaling);
    runKernel->resolution_history->input_crop.bottom = prevRunKernel->resolution_history->input_crop.bottom +
        static_cast<uint32_t>(prevRunKernel->resolution_info->input_crop.bottom / _sensorVerticalScaling);

    if (updateResolution == true)
    {
        runKernel->resolution_history->output_width = runKernel->resolution_info->input_width;
        runKernel->resolution_history->output_height = runKernel->resolution_info->input_height;
    }

    return StaticGraphStatus::SG_OK;
}

StaticGraphRunKernel* GraphResolutionConfigurator::getRunKernel(RunKernelCoords& coord)
{
    GraphTopology* graphTopology = nullptr;
    StaticGraphStatus status = _staticGraph->getGraphTopology(&graphTopology);

    if (status != StaticGraphStatus::SG_OK) {
        return nullptr;
    }

    auto node = graphTopology->links[coord.nodeInd]->destNode;
    return &node->nodeKernels.kernelList[coord.kernelInd].run_kernel;
}

StaticGraphStatus GraphResolutionConfigurator::findRunKernel(uint32_t kernelUuid, RunKernelCoords& coord)
{
    GraphTopology* graphTopology = nullptr;
    StaticGraphStatus status = _staticGraph->getGraphTopology(&graphTopology);

    if (status != StaticGraphStatus::SG_OK) {
        return StaticGraphStatus::SG_ERROR;
    }

    for (int32_t i = 0; i < graphTopology->numOfLinks; i++)
    {
        auto node = graphTopology->links[i]->destNode;
        if (node != nullptr)
        {
            for (uint32_t j = 0; j < node->nodeKernels.kernelCount; j++)
            {
                if (node->nodeKernels.kernelList[j].run_kernel.kernel_uuid == kernelUuid)
                {
                    //return &node->nodeKernels.kernelList[j].run_kernel;
                    coord.nodeInd = i;
                    coord.kernelInd = j;
                    return StaticGraphStatus::SG_OK;
                }
            }
        }
    }

    return StaticGraphStatus::SG_ERROR;
}

StaticGraphStatus GraphResolutionConfigurator::initRunKernelCoord(GraphResolutionConfiguratorKernelRole role, RunKernelCoords &coord)
{
    uint32_t kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuid(role);
    return findRunKernel(kernelUuid, coord);
}

StaticGraphStatus GraphResolutionConfigurator::initOutputRunKernelCoord(RunKernelCoords& coord)
{
    GraphTopology* graphTopology = nullptr;
    StaticGraphStatus status = _staticGraph->getGraphTopology(&graphTopology);
    GraphLink** links = graphTopology->links;

    int32_t graphId;
    _staticGraph->getGraphId(&graphId);

    HwSink hwSink = HwSink::Disconnected;

    // Try to get output resolution according to priority - first preview then video or stills
    std::vector<VirtualSink> virtualSinks;
    virtualSinks.push_back(VirtualSink::PreviewSink);
    virtualSinks.push_back(VirtualSink::VideoSink);
    virtualSinks.push_back(VirtualSink::StillsSink);

    for (auto virtualSink : virtualSinks)
    {
        status = _staticGraph->getVirtualSinkConnection(virtualSink, &hwSink);

        if (status != StaticGraphStatus::SG_OK) {
            return StaticGraphStatus::SG_ERROR;
        }

        if (hwSink != HwSink::Disconnected)
        {
            // found it
            break;
        }
    }

    if (hwSink == HwSink::Disconnected)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Find output device
    auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId, links);
    return findRunKernel(kernelUuid, coord);
}

StaticGraphStatus GraphResolutionConfigurator::initKernelCoordsForUpdate()
{
    std::vector<uint32_t> kernelUuids;

    GraphResolutionConfiguratorHelper::getRunKernelUuidForResHistoryUpdate(kernelUuids);

    for (auto& kernelUuid : kernelUuids)
    {
        RunKernelCoords coord;
        if (findRunKernel(kernelUuid, coord) == StaticGraphStatus::SG_OK)
        {
            _kernelsForUpdate.push_back(coord);
        }
    }

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::getInputRoiForOutput(const ResolutionRoi& roi, HwSink hwSink, SensorRoi& sensorRoi)
{
     if (_staticGraph == nullptr)
     {
         return StaticGraphStatus::SG_ERROR;
     }

     int32_t graphId;
     _staticGraph->getGraphId(&graphId);

     GraphTopology* graphTopology = nullptr;
     _staticGraph->getGraphTopology(&graphTopology);

     GraphLink** links = graphTopology->links;

     auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId, links);
     RunKernelCoords coord;
     if (findRunKernel(kernelUuid, coord) != StaticGraphStatus::SG_OK)
     {
         return StaticGraphStatus::SG_ERROR;
     }

     auto outputRunKernel = getRunKernel(coord);

     // We want to calculate the scaling ratio without taking any cropping into consideration
     double widthIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_width -
         outputRunKernel->resolution_history->input_crop.left -
         outputRunKernel->resolution_history->input_crop.right) / outputRunKernel->resolution_history->output_width;

     double heightIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_height -
         outputRunKernel->resolution_history->input_crop.top -
         outputRunKernel->resolution_history->input_crop.bottom) / outputRunKernel->resolution_history->output_height;

     StaticGraphKernelResCrop outputCropHist = outputRunKernel->resolution_history->input_crop;
     StaticGraphKernelResCrop outputCrop = { 0,0,0,0 };
     if (outputRunKernel->resolution_info != nullptr)
     {
         outputCrop = outputRunKernel->resolution_info->input_crop;

         // Translate crop to sensor units, w/out this kernel's scaling since input crop is done before scaling.
         outputCrop.left += static_cast<int32_t>(outputCrop.left * widthIn2OutScale);
         outputCrop.right += static_cast<int32_t>(outputCrop.right * widthIn2OutScale);
         outputCrop.top += static_cast<int32_t>(outputCrop.top * heightIn2OutScale);
         outputCrop.bottom += static_cast<int32_t>(outputCrop.bottom * heightIn2OutScale);

         widthIn2OutScale *= static_cast<double>(outputRunKernel->resolution_info->input_width -
             outputRunKernel->resolution_info->input_crop.left -
             outputRunKernel->resolution_info->input_crop.right) / outputRunKernel->resolution_info->output_width;

         heightIn2OutScale *= static_cast<double>(outputRunKernel->resolution_info->input_height -
             outputRunKernel->resolution_info->input_crop.top -
             outputRunKernel->resolution_info->input_crop.bottom) / outputRunKernel->resolution_info->output_height;
     }

     // Now remove any scaling done by sensor itself
     widthIn2OutScale *= _sensorHorizontalScaling;
     heightIn2OutScale *= _sensorVerticalScaling;

     if ((outputCropHist.left < _sensorHorizontalCropLeft) ||
        (outputCropHist.right < _sensorHorizontalCropRight) ||
        (outputCropHist.top < _sensorVerticalCropTop) ||
        (outputCropHist.bottom < _sensorVerticalCropBottom)) {
         return StaticGraphStatus::SG_ERROR;
     }

     // Translate to ROI on input
     sensorRoi.width = GRA_ROUND_UP(static_cast<uint32_t>(roi.width * widthIn2OutScale), 2);
     sensorRoi.height = GRA_ROUND_UP(static_cast<uint32_t>(roi.height * heightIn2OutScale), 2);
     sensorRoi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>((roi.left * widthIn2OutScale) + ( (outputCropHist.left - _sensorHorizontalCropLeft) * _sensorHorizontalScaling ) + outputCrop.left), 2);
     sensorRoi.cropRight = GRA_ROUND_UP(static_cast<uint32_t>((roi.right * widthIn2OutScale) + ( (outputCropHist.right - _sensorHorizontalCropRight) * _sensorHorizontalScaling ) + outputCrop.right), 2);
     sensorRoi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>((roi.top * heightIn2OutScale) + ( ( outputCropHist.top - _sensorVerticalCropTop) * _sensorVerticalScaling ) + outputCrop.top), 2);
     sensorRoi.cropBottom = GRA_ROUND_UP(static_cast<uint32_t>((roi.bottom * heightIn2OutScale) + ( ( outputCropHist.bottom - _sensorVerticalCropBottom ) * _sensorVerticalScaling ) + outputCrop.bottom), 2);

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::getStatsRoiFromSensorRoi(const SensorRoi& sensorRoi, const HwSink hwSink, ResolutionRoi& statsRoi)
{
    int32_t graphId;
    _staticGraph->getGraphId(&graphId);

    GraphTopology* graphTopology = nullptr;
    _staticGraph->getGraphTopology(&graphTopology);

     GraphLink** links = graphTopology->links;

    auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId, links);
    RunKernelCoords coord;

    kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(HwSink::AeOutSink, graphId, links);
    if (findRunKernel(kernelUuid, coord) != StaticGraphStatus::SG_OK)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    auto aeRunKernel = getRunKernel(coord);
    if (aeRunKernel == nullptr) {
        return StaticGraphStatus::SG_ERROR;
    }

    StaticGraphKernelResCrop aeCrop;
    if (aeRunKernel->resolution_info != nullptr) {
        aeCrop = aeRunKernel->resolution_info->input_crop;
    }
    StaticGraphKernelResCrop aeCropHist = aeRunKernel->resolution_history->input_crop;

    // Compute stat ROI relative to sensor roi

    double widthIn2OutScale = static_cast<double>(aeRunKernel->resolution_history->output_width) /
        (aeRunKernel->resolution_history->input_width - aeCropHist.left - aeCropHist.right);

    double heightIn2OutScale = static_cast<double>(aeRunKernel->resolution_history->output_height) /
        (aeRunKernel->resolution_history->input_height - aeCropHist.top - aeCropHist.bottom);

    statsRoi.width = GRA_ROUND_UP(static_cast<uint32_t>(sensorRoi.width * widthIn2OutScale), 2);
    statsRoi.height = GRA_ROUND_UP(static_cast<uint32_t>(sensorRoi.height * heightIn2OutScale), 2);
    statsRoi.left = GRA_ROUND_UP(static_cast<uint32_t>((sensorRoi.cropLeft * widthIn2OutScale) + aeCropHist.left), 2);
    statsRoi.right = GRA_ROUND_UP(static_cast<uint32_t>((sensorRoi.cropRight * widthIn2OutScale) + aeCropHist.right), 2);
    statsRoi.top = GRA_ROUND_UP(static_cast<uint32_t>((sensorRoi.cropTop * heightIn2OutScale) + aeCropHist.top), 2);
    statsRoi.bottom = GRA_ROUND_UP(static_cast<uint32_t>((sensorRoi.cropBottom * heightIn2OutScale) + aeCropHist.bottom), 2);

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::undoSensorCropandScale(SensorRoi& sensor_roi)
{
    sensor_roi.width = GRA_ROUND_UP(static_cast<uint32_t>(sensor_roi.width / _sensorHorizontalScaling), 2);
    sensor_roi.height = GRA_ROUND_UP(static_cast<uint32_t>(sensor_roi.height / _sensorVerticalScaling), 2);
    sensor_roi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>((sensor_roi.cropLeft / _sensorHorizontalScaling) + _sensorHorizontalCropLeft), 2);
    sensor_roi.cropRight = GRA_ROUND_UP(static_cast<uint32_t>((sensor_roi.cropRight / _sensorHorizontalScaling) + _sensorHorizontalCropRight), 2);
    sensor_roi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>((sensor_roi.cropTop / _sensorVerticalScaling) + _sensorVerticalCropTop), 2);
    sensor_roi.cropBottom = GRA_ROUND_UP(static_cast<uint32_t>((sensor_roi.cropBottom / _sensorVerticalScaling) + _sensorVerticalCropBottom ), 2);

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::sensorCropOrScaleExist(bool& sensor_crop_or_scale_exist)
{
    sensor_crop_or_scale_exist = false;
    if ((abs(_sensorHorizontalScaling - 1.0F) < 0.01F) ||
        (abs(_sensorVerticalScaling - 1.0F) < 0.01F) ||
        (_sensorHorizontalCropLeft > 0) ||
        (_sensorHorizontalCropRight > 0) ||
        (_sensorVerticalCropTop > 0) ||
        (_sensorVerticalCropBottom > 0))
    {
        sensor_crop_or_scale_exist = true;
    }

    return StaticGraphStatus::SG_OK;
}

#if SUPPORT_KEY_RESOLUTIONS == 0
//
//      IPU 8
//

Ipu8GraphResolutionConfigurator::Ipu8GraphResolutionConfigurator(IStaticGraphConfig* staticGraph):
    GraphResolutionConfigurator(staticGraph)
{
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::updateStaticGraphConfig(const RegionOfInterest& roi, bool isCenteredZoom)
{
    if (_staticGraph == nullptr)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Get ROI in sensor dimensions

    // If zoom is centered, calculate the pan & tilt
    RegionOfInterest userRoi = roi;

    if (isCenteredZoom == true)
    {
        userRoi.panFactor = (1 - userRoi.zoomFactor) / 2;
        userRoi.tiltFactor = (1 - userRoi.zoomFactor) / 2;
    }

    SensorRoi sensorRoi;
    if (getSensorRoi(userRoi, sensorRoi) != StaticGraphStatus::SG_OK)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    //
    // Step #2 Dynamic update according to this ROI
    //
    return updateRunKernelOfScalers(sensorRoi);
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::updateRunKernelOfScalers(SensorRoi& roi)
{
    StaticGraphStatus ret = StaticGraphStatus::SG_OK;

    auto downscalerRunKernel = getRunKernel(_downscalerRunKernelCoord);
    auto upscalerRunKernel = getRunKernel(_upscalerRunKernelCoord);
    auto cropperRunKernel = getRunKernel(_cropperRunKernelCoord);

    uint32_t fragments = 1;
    auto downscalerFragments = getKernelFragments(_downscalerRunKernelCoord);
    auto upscalerFragments = getKernelFragments(_upscalerRunKernelCoord);
    auto cropperFragments = getKernelFragments(_cropperRunKernelCoord);

    uint32_t inputWidth = downscalerRunKernel->resolution_info->input_width;
    uint32_t inputHeight = downscalerRunKernel->resolution_info->input_height;

    uint32_t outputWidth = cropperRunKernel->resolution_info->output_width;
    uint32_t outputHeight = cropperRunKernel->resolution_info->output_height;

    StaticGraphKernelResCrop downscalerCropHistory;
    downscalerCropHistory.left = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.left - static_cast<int32_t>(_sensorHorizontalCropLeft)) * _sensorHorizontalScaling);
    downscalerCropHistory.right = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.right - static_cast<int32_t>(_sensorHorizontalCropRight)) * _sensorHorizontalScaling);
    downscalerCropHistory.top = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.top - static_cast<int32_t>(_sensorVerticalCropTop)) * _sensorVerticalScaling);
    downscalerCropHistory.bottom = static_cast<int32_t>((downscalerRunKernel->resolution_history->input_crop.bottom - static_cast<int32_t>(_sensorVerticalCropBottom)) * _sensorVerticalScaling);

    // If ROI is larger than scaler's output resolution - we downscale
    if (roi.width >= outputWidth)
    {
        // Only down scaler is active
        if (updateRunKernelDownScaler(downscalerRunKernel, roi, inputWidth, inputHeight,
            outputWidth, outputHeight, &downscalerCropHistory) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }

        updateRunKernelPassThrough(cropperRunKernel, outputWidth, outputHeight);
        updateRunKernelPassThrough(upscalerRunKernel, outputWidth, outputHeight);
    }
    else
    {
        updateRunKernelPassThrough(downscalerRunKernel, inputWidth, inputHeight);

        // Configure ESPA crop to output resolution (TNR ROI)
        updateRunKernelCropper(cropperRunKernel, inputWidth, inputHeight, outputWidth, outputHeight, &downscalerCropHistory);

        // Configure upscaler to crop from output resolution to ROI
        updateRunKernelUpScaler(upscalerRunKernel, roi, cropperRunKernel, outputWidth, outputHeight, &downscalerCropHistory);
    }

    // Update resolution histories according to decisions made above
    if (updateRunKernelResolutionHistory(upscalerRunKernel, downscalerRunKernel) != StaticGraphStatus::SG_OK)
    {
        ret = StaticGraphStatus::SG_ERROR;
    }

    if (updateRunKernelResolutionHistory(cropperRunKernel, upscalerRunKernel) != StaticGraphStatus::SG_OK)
    {
        ret = StaticGraphStatus::SG_ERROR;
    }

    // Update resolution history for relevant kernels
    for (auto& runKernelForUpdate : _kernelsForUpdate)
    {
        // We update all histories according to upscaler... assuming no more cropping from now on :(
        StaticGraphRunKernel* runKernelPtr = getRunKernel(runKernelForUpdate);
        if (updateRunKernelResolutionHistory(runKernelPtr, upscalerRunKernel, false) != StaticGraphStatus::SG_OK)
        {
            ret = StaticGraphStatus::SG_ERROR;
        }
    }

    // :TODO: Striping Support
    // Currently we assume one stripe.
    updateKernelFragments(downscalerRunKernel, downscalerFragments, fragments);
    updateKernelFragments(upscalerRunKernel, upscalerFragments, fragments);
    updateKernelFragments(cropperRunKernel, cropperFragments, fragments);

    if (ret == StaticGraphStatus::SG_OK)
    {
        ret = SanityCheck();
    }

    return ret;
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::updateRunKernelCropper(StaticGraphRunKernel* runKernel,
    uint32_t inputWidth, uint32_t inputHeight,
    uint32_t outputWidth, uint32_t outputHeight,
    StaticGraphKernelResCrop* originalDownscalerCrop)
{

    runKernel->resolution_info->input_width = inputWidth;
    runKernel->resolution_info->input_height = inputHeight;

    runKernel->resolution_info->output_width = outputWidth;
    runKernel->resolution_info->output_height = outputHeight;

    runKernel->resolution_info->input_crop.left = 0;
    runKernel->resolution_info->input_crop.right = 0;
    runKernel->resolution_info->input_crop.top = 0;
    runKernel->resolution_info->input_crop.bottom = 0;

    // Configure to crop the required amount. First try to use the original DS cropping (Remove padding)
    int32_t totalHorizontalCrop = inputWidth - outputWidth;

    int32_t originalDsPadding = originalDownscalerCrop->left < 0 ? -originalDownscalerCrop->left : 0;
    if (totalHorizontalCrop >= originalDsPadding && originalDsPadding > 0)
    {
        runKernel->resolution_info->input_crop.left = originalDsPadding;
        totalHorizontalCrop -= originalDsPadding;

        // Padding was handled, no need to handle again
        originalDownscalerCrop->left = 0;
    }

    originalDsPadding = originalDownscalerCrop->right < 0 ? -originalDownscalerCrop->right : 0;
    if (totalHorizontalCrop >= originalDsPadding && originalDsPadding > 0)
    {
        runKernel->resolution_info->input_crop.right = originalDsPadding;
        totalHorizontalCrop -= originalDsPadding;

        // Padding was handled, no need to handle again
        originalDownscalerCrop->right = 0;
    }

    // Now crop symmetrically to TNR size if any more cropping is required
    runKernel->resolution_info->input_crop.left += GRA_ROUND_DOWN(static_cast<uint32_t>(static_cast<double>(totalHorizontalCrop)) / 2, 2);
    runKernel->resolution_info->input_crop.right += GRA_ROUND_UP(static_cast<uint32_t>(static_cast<double>(totalHorizontalCrop)) / 2, 2);

    // Configure to crop the required amount. First try to use the original DS cropping (Remove padding)
    int32_t totalVerticalCrop = inputHeight - outputHeight;

    originalDsPadding = originalDownscalerCrop->top < 0 ? -originalDownscalerCrop->top : 0;
    if (totalVerticalCrop >= originalDsPadding && originalDsPadding > 0)
    {
        runKernel->resolution_info->input_crop.top = originalDsPadding;
        totalVerticalCrop -= originalDsPadding;

        // Padding was handled, no need to handle again
        originalDownscalerCrop->top = 0;
    }

    originalDsPadding = originalDownscalerCrop->bottom < 0 ? -originalDownscalerCrop->bottom : 0;
    if (totalVerticalCrop >= originalDsPadding && originalDsPadding > 0)
    {
        runKernel->resolution_info->input_crop.bottom = originalDsPadding;
        totalVerticalCrop -= originalDsPadding;

        // Padding was handled, no need to handle again
        originalDownscalerCrop->bottom = 0;
    }

    // Now crop symmetrically to TNR size if any more cropping is required
    runKernel->resolution_info->input_crop.top += GRA_ROUND_DOWN(static_cast<uint32_t>(static_cast<double>(totalVerticalCrop)) / 2, 2);
    runKernel->resolution_info->input_crop.bottom += GRA_ROUND_UP(static_cast<uint32_t>(static_cast<double>(totalVerticalCrop)) / 2, 2);

    // Update the left crop in striping system api. Currently assuming one stripe
    StaticGraphKernelSystemApiIoBuffer1_4* systemApi = nullptr;
    if (runKernel->system_api.size != ((GRA_ROUND_UP(sizeof(SystemApiRecordHeader), 4)) + (sizeof(StaticGraphKernelSystemApiIoBuffer1_4))))
    {
        // TODO log error
        return StaticGraphStatus::SG_ERROR;
    }

    auto systemApiHeader = static_cast<SystemApiRecordHeader*>(runKernel->system_api.data);
    if (systemApiHeader->systemApiUuid != GraphResolutionConfiguratorHelper::getRunKernelIoBufferSystemApiUuid())
    {
        // TODO log error
        return StaticGraphStatus::SG_ERROR;
    }

    systemApi = reinterpret_cast<StaticGraphKernelSystemApiIoBuffer1_4*>(static_cast<int8_t*>(runKernel->system_api.data) + GRA_ROUND_UP(sizeof(SystemApiRecordHeader), 4));
    systemApi->x_output_offset_per_stripe[0] = runKernel->resolution_info->input_crop.left;

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::updateRunKernelUpScaler(StaticGraphRunKernel* runKernel, SensorRoi& roi, StaticGraphRunKernel* cropperRunKernel,
    uint32_t outputWidth, uint32_t outputHeight, StaticGraphKernelResCrop* originalDownscalerCrop)
{
    StaticGraphStatus ret = StaticGraphStatus::SG_OK;

    runKernel->resolution_info->input_width = cropperRunKernel->resolution_info->output_width;
    runKernel->resolution_info->input_height = cropperRunKernel->resolution_info->output_height;
    runKernel->resolution_info->output_width = outputWidth;
    runKernel->resolution_info->output_height = outputHeight;

    runKernel->resolution_info->input_crop.left = GRA_ROUND_UP(roi.cropLeft - cropperRunKernel->resolution_info->input_crop.left - originalDownscalerCrop->left, 2);
    runKernel->resolution_info->input_crop.right = GRA_ROUND_UP(roi.cropRight - cropperRunKernel->resolution_info->input_crop.right - originalDownscalerCrop->right, 2);
    runKernel->resolution_info->input_crop.top = GRA_ROUND_UP(roi.cropTop - cropperRunKernel->resolution_info->input_crop.top - originalDownscalerCrop->top, 2);
    runKernel->resolution_info->input_crop.bottom = GRA_ROUND_UP(roi.cropBottom - cropperRunKernel->resolution_info->input_crop.bottom - originalDownscalerCrop->bottom, 2);

    return ret;
}

StaticGraphFragmentDesc* Ipu8GraphResolutionConfigurator::getKernelFragments(RunKernelCoords& coord)
{
    GraphTopology* graphTopology = nullptr;
    StaticGraphStatus status = _staticGraph->getGraphTopology(&graphTopology);

    if (status != StaticGraphStatus::SG_OK) {
        return nullptr;
    }

    auto node = graphTopology->links[coord.nodeInd]->destNode;
    return node->nodeKernels.kernelList[coord.kernelInd].fragment_descs;
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::updateKernelFragments(StaticGraphRunKernel* runKernel, StaticGraphFragmentDesc* fragmentsDesc, uint32_t fragments)
{
    if (runKernel == nullptr || fragmentsDesc == nullptr)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    if (fragments == 1)
    {
        // Nothing to do
        return StaticGraphStatus::SG_OK;
    }

    // Add support here
    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::SanityCheck()
{
    auto downscalerRunKernel = getRunKernel(_downscalerRunKernelCoord);
    auto upscalerRunKernel = getRunKernel(_upscalerRunKernelCoord);
    auto cropperRunKernel = getRunKernel(_cropperRunKernelCoord);

    // Resolution hist output must be same as info input
    if (downscalerRunKernel->resolution_info->input_width != downscalerRunKernel->resolution_history->output_width ||
        downscalerRunKernel->resolution_info->input_height != downscalerRunKernel->resolution_history->output_height ||

        upscalerRunKernel->resolution_info->input_width != upscalerRunKernel->resolution_history->output_width ||
        upscalerRunKernel->resolution_info->input_height != upscalerRunKernel->resolution_history->output_height ||

        cropperRunKernel->resolution_info->input_width != cropperRunKernel->resolution_history->output_width ||
        cropperRunKernel->resolution_info->input_height != cropperRunKernel->resolution_history->output_height)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Resolution consistency
    if (downscalerRunKernel->resolution_info->output_width != cropperRunKernel->resolution_info->input_width ||
        downscalerRunKernel->resolution_info->output_height != cropperRunKernel->resolution_info->input_height ||

        cropperRunKernel->resolution_info->output_width != upscalerRunKernel->resolution_info->input_width ||
        cropperRunKernel->resolution_info->output_height != upscalerRunKernel->resolution_info->input_height)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Cropping values

    if (SanityCheckCrop(&downscalerRunKernel->resolution_info->input_crop) == StaticGraphStatus::SG_ERROR ||
        SanityCheckCrop(&upscalerRunKernel->resolution_info->input_crop) == StaticGraphStatus::SG_ERROR ||
        SanityCheckCrop(&cropperRunKernel->resolution_info->input_crop) == StaticGraphStatus::SG_ERROR)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Make sure DS is actually down scaling
    int32_t widthAfterCrop = downscalerRunKernel->resolution_info->input_width - downscalerRunKernel->resolution_info->input_crop.left - downscalerRunKernel->resolution_info->input_crop.right;
    int32_t heightAfterCrop = downscalerRunKernel->resolution_info->input_height - downscalerRunKernel->resolution_info->input_crop.top - downscalerRunKernel->resolution_info->input_crop.bottom;

     if (widthAfterCrop < 0 || widthAfterCrop < downscalerRunKernel->resolution_info->output_width ||
         heightAfterCrop < 0 || heightAfterCrop < downscalerRunKernel->resolution_info->output_height ||
         static_cast<double>(widthAfterCrop) / downscalerRunKernel->resolution_info->output_width > 16)
     {
         return StaticGraphStatus::SG_ERROR;
     }

    // Make sure US is actually up scaling
    widthAfterCrop = upscalerRunKernel->resolution_info->input_width - upscalerRunKernel->resolution_info->input_crop.left - upscalerRunKernel->resolution_info->input_crop.right;
    heightAfterCrop = upscalerRunKernel->resolution_info->input_height - upscalerRunKernel->resolution_info->input_crop.top - upscalerRunKernel->resolution_info->input_crop.bottom;

    if (widthAfterCrop < 0 || widthAfterCrop > upscalerRunKernel->resolution_info->output_width ||
        heightAfterCrop < 0 || heightAfterCrop > upscalerRunKernel->resolution_info->output_height ||
        static_cast<double>(upscalerRunKernel->resolution_history->output_width) / widthAfterCrop > 16)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    // Make sure cropper is actually cropping
    widthAfterCrop = cropperRunKernel->resolution_info->input_width - cropperRunKernel->resolution_info->input_crop.left - cropperRunKernel->resolution_info->input_crop.right;
    heightAfterCrop = cropperRunKernel->resolution_info->input_height - cropperRunKernel->resolution_info->input_crop.top - cropperRunKernel->resolution_info->input_crop.bottom;

    if (widthAfterCrop < 0 || widthAfterCrop != cropperRunKernel->resolution_info->output_width ||
        heightAfterCrop < 0 || heightAfterCrop != cropperRunKernel->resolution_info->output_height)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus Ipu8GraphResolutionConfigurator::SanityCheckCrop(StaticGraphKernelResCrop* crop)
{
    if (crop->top < 0 || crop->bottom < 0 || crop->left < 0 || crop->right < 0 ||
        crop->top & 1 || crop->bottom & 1 || crop->left & 1 || crop->right & 1)
    {
        return StaticGraphStatus::SG_ERROR;
    }

    return StaticGraphStatus::SG_OK;
}

#endif
