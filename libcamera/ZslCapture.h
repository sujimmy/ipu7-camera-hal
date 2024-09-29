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

#pragma once

#include <map>

#include <libcamera/controls.h>
#include <libcamera/base/mutex.h>

namespace libcamera {

struct ZslInfo {
    bool isManualExposure;
    bool isAeStable;
    bool isAfStable;
    bool isAwbStable;

    int64_t timestamp;
    int64_t sequence;

    ZslInfo() {
        isManualExposure = false;
        isAeStable = false;
        isAfStable = false;
        isAwbStable = false;

        timestamp = 0;
        sequence = -1;
    }
};

class ZslCapture {
 public:
    ZslCapture();
    ~ZslCapture();

    void registerFrameInfo(unsigned int frameNumber, const ControlList& controls);

    void updateTimeStamp(unsigned int frameNumber, uint64_t timestamp);
    void updateSequence(unsigned int frameNumber, int64_t sequence);
    void update3AStatus(unsigned int frameNumber, const ControlList& metadata);

    void getZslSequenceAndTimestamp(uint64_t& timestamp, int64_t& sequence);

 private:
    bool isManualExposureSettings(const ControlList& controls);

    mutable Mutex mMutex;

    static const uint8_t kMaxZslRequest = 24;
    /* first: frame number, second: ZslInfo */
    std::map<unsigned int, ZslInfo> mZslInfoMap;
};

} /* namespace libcamera */
