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

    _widthIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_width -
        outputRunKernel->resolution_history->input_crop.left -
        outputRunKernel->resolution_history->input_crop.right) /
        outputRunKernel->resolution_history->output_width;

    _heightIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_height -
        outputRunKernel->resolution_history->input_crop.top -
        outputRunKernel->resolution_history->input_crop.bottom) / outputRunKernel->resolution_history->output_height;

    _originalOutputCrop = outputRunKernel->resolution_history->input_crop;

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

    _originalOutputCrop.top = static_cast<uint32_t>(_originalOutputCrop.top * _sensorHorizontalScaling);
    _originalOutputCrop.bottom = static_cast<uint32_t>(_originalOutputCrop.bottom * _sensorHorizontalScaling);
    _originalOutputCrop.left = static_cast<uint32_t>(_originalOutputCrop.left * _sensorVerticalScaling);
    _originalOutputCrop.right = static_cast<uint32_t>(_originalOutputCrop.right * _sensorVerticalScaling);
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

    // Calculate required scaling according to ROI
    return updateRunKernelOfScalers(sensorRoi);
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
        // Calculate ROI on input
        // Important: This will result in sensorRoi in aspect ratio of *sensor* instead of aspect ratio of *output*
        // It will lead to wrong scaling (ds or us) in one of the dimensions
        // Need to adjust to output aspect ratio if this feature is actually needed in the future
        sensorRoi.width = GRA_ROUND_UP(static_cast<uint32_t>(outputRunKernel->resolution_history->input_width * userRoi.zoomFactor), 2);
        sensorRoi.height = GRA_ROUND_UP(static_cast<uint32_t>(outputRunKernel->resolution_history->input_height * userRoi.zoomFactor), 2);

        sensorRoi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>(outputRunKernel->resolution_history->input_width * userRoi.panFactor), 2);
        sensorRoi.cropRight = GRA_ROUND_DOWN(static_cast<uint32_t>(outputRunKernel->resolution_history->input_width * (1 - userRoi.panFactor - userRoi.zoomFactor)), 2);

        sensorRoi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>(outputRunKernel->resolution_history->input_height * userRoi.tiltFactor), 2);
        sensorRoi.cropBottom = GRA_ROUND_DOWN(static_cast<uint32_t>(outputRunKernel->resolution_history->input_height * (1 - userRoi.tiltFactor - userRoi.zoomFactor)), 2);

        return StaticGraphStatus::SG_OK;
    }

    // Calculate ROI on output, using original output since user ROI is relative to full output ROI (not after some zoom was performed)
    uint32_t outputLeft = static_cast<uint32_t>(outputRunKernel->resolution_info->output_width * userRoi.panFactor);
    uint32_t outputRight = static_cast<uint32_t>(outputRunKernel->resolution_info->output_width * (1 - userRoi.panFactor - userRoi.zoomFactor));
    uint32_t outputTop = static_cast<uint32_t>(outputRunKernel->resolution_info->output_height * userRoi.tiltFactor);
    uint32_t outputBottom = static_cast<uint32_t>(outputRunKernel->resolution_info->output_height * (1 - userRoi.tiltFactor - userRoi.zoomFactor));

    uint32_t outputWidth = outputRunKernel->resolution_info->output_width - outputLeft - outputRight;
    uint32_t outputHeight = outputRunKernel->resolution_info->output_height - outputTop - outputBottom;

    // Now add the crop that is being performed by this output drainer to the crop value
    outputLeft += outputRunKernel->resolution_info->input_crop.left;
    outputRight += outputRunKernel->resolution_info->input_crop.right;
    outputTop += outputRunKernel->resolution_info->input_crop.top;
    outputBottom += outputRunKernel->resolution_info->input_crop.bottom;

    // Translate to ROI on input
    sensorRoi.width = GRA_ROUND_UP(static_cast<uint32_t>(outputWidth * _widthIn2OutScale), 2);
    sensorRoi.height = GRA_ROUND_UP(static_cast<uint32_t>(outputHeight * _heightIn2OutScale), 2);
    sensorRoi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>((outputLeft * _widthIn2OutScale) + _originalOutputCrop.left), 2);
    sensorRoi.cropRight = GRA_ROUND_UP(static_cast<uint32_t>(((outputRight * _widthIn2OutScale) + _originalOutputCrop.right)), 2);
    sensorRoi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>((outputTop * _heightIn2OutScale) + _originalOutputCrop.top), 2);
    sensorRoi.cropBottom = GRA_ROUND_UP(static_cast<uint32_t>((outputBottom * _heightIn2OutScale) + _originalOutputCrop.bottom), 2);

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
    downscalerCropHistory.left = static_cast<uint32_t>(downscalerRunKernel->resolution_history->input_crop.left * _sensorHorizontalScaling);
    downscalerCropHistory.right = static_cast<uint32_t>(downscalerRunKernel->resolution_history->input_crop.right * _sensorHorizontalScaling);
    downscalerCropHistory.top = static_cast<uint32_t>(downscalerRunKernel->resolution_history->input_crop.top * _sensorVerticalScaling);
    downscalerCropHistory.bottom = static_cast<uint32_t>(downscalerRunKernel->resolution_history->input_crop.bottom * _sensorVerticalScaling);

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
    }
    else
    {
        // Configure downscaler and upscaler according to upscaler constraints

        // Update upscaler info, according constraints. Returns the expected input width and height for upscaler.
        uint32_t upscalerActualInputWidth;
        uint32_t upscalerActualInputHeight;
        if (updateRunKernelUpScaler(upscalerRunKernel, roi.width, roi.height, outputWidth, outputHeight,
            upscalerActualInputWidth, upscalerActualInputHeight) != StaticGraphStatus::SG_OK)
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
    }

    // Cropper is not a part of dynamic scaling, even if it was a part of static configuration.
    updateRunKernelPassThrough(cropperRunKernel, outputWidth, outputHeight);

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
    runKernel->resolution_info->input_crop.left = GRA_ROUND_DOWN(roi.cropLeft - originalScalerCrop->left, 2);
    runKernel->resolution_info->input_crop.right = GRA_ROUND_DOWN(roi.cropRight - originalScalerCrop->right, 2);
    runKernel->resolution_info->input_crop.top = GRA_ROUND_DOWN(roi.cropTop - originalScalerCrop->top, 2);
    runKernel->resolution_info->input_crop.bottom = GRA_ROUND_DOWN(roi.cropBottom - originalScalerCrop->bottom, 2);

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
    uint32_t& upscalerActualInputWidth, uint32_t& upscalerActualInputHeight)
{
    static const int SCALE_PREC = 16;
    const uint32_t ia_pal_isp_upscaler_1_0__scaling_ratio__min = 4096;

    const uint32_t max_upscaling = (1 << SCALE_PREC) / ia_pal_isp_upscaler_1_0__scaling_ratio__min;

    StaticGraphStatus ret = StaticGraphStatus::SG_OK;

     // Find valid output configurations
    uint32_t stepW = 1;
    uint32_t stepH = 1;

    for (stepH = 1; stepH < outputHeight / 2; stepH++)
    {
        double horStep = static_cast<double>(stepH) * outputWidth / 2 / outputHeight;
        if (floor((horStep)) == horStep)
        {
            stepW = static_cast<uint32_t>(horStep) * 2;
            break;
        }
    }

    // The input to the upscaler should be multiple of (stepW, stepH) and also even numbers
    stepW *= 2;
    stepH *= 2;

    upscalerActualInputWidth = GRA_ROUND_DOWN(inputWidth, stepW);
    upscalerActualInputHeight = (upscalerActualInputWidth / stepW) * stepH;

    if (upscalerActualInputWidth == 0 || upscalerActualInputHeight == 0)
    {
        // Could not find a valid configuration, this configuration may fail
        upscalerActualInputWidth = inputWidth;
        upscalerActualInputHeight = inputHeight;

        ret = StaticGraphStatus::SG_ERROR;
    }

    if ((outputWidth / upscalerActualInputWidth) > max_upscaling)
    {
        // Perform the max possible up scaling, downscaler will adjust itself
        upscalerActualInputWidth = outputWidth / max_upscaling;
        upscalerActualInputWidth = GRA_ROUND_UP(upscalerActualInputWidth, stepW);
        upscalerActualInputHeight = (upscalerActualInputWidth / stepW) * stepH;
    }

    //
    // Configure scaler
    //
    runKernel->resolution_info->input_width = upscalerActualInputWidth;
    runKernel->resolution_info->input_height = upscalerActualInputHeight;

    runKernel->resolution_info->output_width = outputWidth;
    runKernel->resolution_info->output_height = outputHeight;

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
    auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId);
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

     auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId);
     RunKernelCoords coord;
     if (findRunKernel(kernelUuid, coord) != StaticGraphStatus::SG_OK)
     {
         return StaticGraphStatus::SG_ERROR;
     }

     auto outputRunKernel = getRunKernel(coord);

     double widthIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_width -
         outputRunKernel->resolution_history->input_crop.left -
         outputRunKernel->resolution_history->input_crop.right) /
         outputRunKernel->resolution_history->output_width;

     double heightIn2OutScale = static_cast<double>(outputRunKernel->resolution_history->input_height -
         outputRunKernel->resolution_history->input_crop.top -
         outputRunKernel->resolution_history->input_crop.bottom) / outputRunKernel->resolution_history->output_height;

     widthIn2OutScale *= _sensorHorizontalScaling;
     heightIn2OutScale *= _sensorVerticalScaling;

     StaticGraphKernelResCrop outputCropHist = outputRunKernel->resolution_history->input_crop;
     StaticGraphKernelResCrop outputCrop = outputRunKernel->resolution_info->input_crop;

     // Translate to ROI on input
     sensorRoi.width = GRA_ROUND_UP(static_cast<uint32_t>(roi.width * widthIn2OutScale), 2);
     sensorRoi.height = GRA_ROUND_UP(static_cast<uint32_t>(roi.height * heightIn2OutScale), 2);
     sensorRoi.cropLeft = GRA_ROUND_UP(static_cast<uint32_t>((roi.left * widthIn2OutScale) + ( outputCropHist.left * _sensorHorizontalScaling ) + (outputCrop.left * widthIn2OutScale)), 2);
     sensorRoi.cropRight = GRA_ROUND_UP(static_cast<uint32_t>((roi.right * widthIn2OutScale) + ( outputCropHist.right * _sensorHorizontalScaling ) + (outputCrop.right * widthIn2OutScale)), 2);
     sensorRoi.cropTop = GRA_ROUND_UP(static_cast<uint32_t>((roi.top * heightIn2OutScale) + ( outputCropHist.top * _sensorVerticalScaling )+ (outputCrop.top * heightIn2OutScale)), 2);
     sensorRoi.cropBottom = GRA_ROUND_UP(static_cast<uint32_t>((roi.bottom * heightIn2OutScale) + ( outputCropHist.bottom * _sensorVerticalScaling ) + (outputCrop.bottom * heightIn2OutScale)), 2);

    return StaticGraphStatus::SG_OK;
}

StaticGraphStatus GraphResolutionConfigurator::getStatsRoiFromSensorRoi(const SensorRoi& sensorRoi, const HwSink hwSink, ResolutionRoi& statsRoi)
{
    int32_t graphId;
    _staticGraph->getGraphId(&graphId);

    auto kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(hwSink, graphId);
    RunKernelCoords coord;

    kernelUuid = GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(HwSink::AeOutSink, graphId);
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
