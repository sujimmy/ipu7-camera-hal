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

#include <libcamera/controls.h>

#include <vector>

namespace libcamera {

class MakerNoteBuilder {
 public:
    MakerNoteBuilder();
    ~MakerNoteBuilder() {}
    // build makernote and write to Jpeg APP2 segment
    void buildMakerNoteMedata(int cameraId, int64_t timestamp, ControlList& metadata);

 private:
    constexpr static unsigned char MAKERNOTE_ID[12] = {
        0x49, 0x6e, 0x74, 0x65, 0x6c, 0x4d,
        0x6b, 0x6e, 0x6f, 0x74, 0x65, 0x0 /* "IntelMknote\0" */};

    std::vector<uint8_t> mMakernoteData;
};

} /* namespace libcamera */
