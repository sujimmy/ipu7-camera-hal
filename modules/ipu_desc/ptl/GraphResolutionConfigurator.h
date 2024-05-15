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

#pragma once
#include <vector>

#define GRA_ROUND_UP(a,b)  (((a) + ((b)-1)) / (b) * (b))
#define GRA_ROUND_DOWN(a,b)  ((a) / (b) * (b))

// ROI in user level
class RegionOfInterest
{
public:
    double zoomFactor;
    double panFactor;
    double tiltFactor;

    // If true, take factors relative to sensor image
    // (needed for WFOV face tracking for example)
    bool fromInput;
};

// ROI translated to sensor resolution
// sensor width = crop.left + width + crop.right
// sensor height = crop.top + height + crop.bottom
class SensorRoi
{
public:
    uint32_t width;         // ROI width
    uint32_t height;        // ROI height
    uint32_t cropLeft;      // Crop from sensor width to ROI left
    uint32_t cropRight;     // Crop from sensor width from ROI right
    uint32_t cropTop;       // Crop from sensor height to ROI top
    uint32_t cropBottom;    // Crop from sensor height from ROI bottom
};

class ResolutionRoi
{
public:
    uint32_t width;         // ROI width
    uint32_t height;        // ROI height
    uint32_t left;          // ROI left point
    uint32_t right;         // ROI right point
    uint32_t top;           // ROI top point
    uint32_t bottom;        // ROI bottom point
};

enum class GraphResolutionConfiguratorKernelRole : uint8_t
{
    UpScaler,
    DownScaler,
    FinalCropper
};

class RunKernelCoords
{
public:
    RunKernelCoords()
    {
        nodeInd = 0;
        kernelInd = 0;
    }
    uint32_t nodeInd;
    uint32_t kernelInd;
};

class GraphResolutionConfigurator
{
public:
    GraphResolutionConfigurator(IStaticGraphConfig* staticGraph);
    ~GraphResolutionConfigurator()
    {
        _kernelsForUpdate.clear();
    }

    StaticGraphStatus updateStaticGraphConfig(const RegionOfInterest& roi,
                                              const RegionOfInterest& prevRoi,
                                              bool isCenteredZoom,
                                              bool prevIsCenteredZoom,
                                              bool& isKeyResolutionChanged);
    // Calculate ROI in sensor dimensions. User ROI is given relative to *full* output ROI
    StaticGraphStatus getSensorRoi(const RegionOfInterest& userRoi, SensorRoi& sensorRoi);
    // Calculate ROI in sensor dimensions. Resolution ROI is given relative to *final* (zoomed) output ROI
    StaticGraphStatus getInputRoiForOutput(ResolutionRoi& roi, HwSink hwSink, SensorRoi& sensorRoi);

private:
    StaticGraphStatus initRunKernelCoord(GraphResolutionConfiguratorKernelRole role, RunKernelCoords& coord);
    StaticGraphStatus initOutputRunKernelCoord(RunKernelCoords& coord);
    StaticGraphStatus initKernelCoordsForUpdate();
    StaticGraphStatus findRunKernel(uint32_t kernelUuid, RunKernelCoords& coord);

    StaticGraphRunKernel* getRunKernel(RunKernelCoords& coord);
    StaticGraphStatus getZoomKeyResolutionIndex(ZoomKeyResolutions* zoomKeyResolutions, SensorRoi sensorRoi, uint32_t& selectedIndex);

    StaticGraphStatus updateRunKernelOfScalers(SensorRoi& roi);

    StaticGraphStatus updateRunKernelDownScaler(StaticGraphRunKernel* runKernel, SensorRoi& roi, uint32_t inputWidth, uint32_t inputHeight,
        uint32_t outputWidth, uint32_t outputHeight, StaticGraphKernelResCrop* originalScalerCrop);
    StaticGraphStatus adjustDownscalerCrop(StaticGraphKernelRes* scalerResInfo);
    StaticGraphStatus updateRunKernelUpScaler(StaticGraphRunKernel* runKernel, uint32_t inputWidth, uint32_t inputHeight,
        uint32_t outputWidth, uint32_t outputHeight, uint32_t& upscalerActualInputWidth, uint32_t& upscalerActualInputHeight);
    StaticGraphStatus updateRunKernelPassThrough(StaticGraphRunKernel* runKernel, uint32_t width, uint32_t height);
    StaticGraphStatus updateCroppingScaler(StaticGraphRunKernel* downscalerRunKernel, StaticGraphRunKernel* upscalerRunKernel);
    StaticGraphStatus updateRunKernelResolutionHistory(StaticGraphRunKernel* runKernel, StaticGraphRunKernel* prevRunKernel, bool updateResolution = true);

    IStaticGraphConfig* _staticGraph;
    RunKernelCoords _downscalerRunKernelCoord;
    RunKernelCoords _upscalerRunKernelCoord;
    RunKernelCoords _cropperRunKernelCoord;
    RunKernelCoords _outputRunKernelCoord;
    std::vector<RunKernelCoords> _kernelsForUpdate;
    double _widthIn2OutScale = 1;
    double _heightIn2OutScale = 1;
    double _sensorScaling = 1;
    StaticGraphKernelResCrop _originalOutputCrop = {0,0,0,0};
};

