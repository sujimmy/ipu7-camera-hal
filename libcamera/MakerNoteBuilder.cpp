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

#include "MakerNoteBuilder.h"

#include <libcamera/base/log.h>
#include <libcamera/control_ids.h>

#include "ParamDataType.h"
#include "PlatformData.h"
#include "Utils.h"
namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

MakerNoteBuilder::MakerNoteBuilder() {
    mMakernoteData.resize(MAKERNOTE_SECTION1_SIZE);
}

void MakerNoteBuilder::buildMakerNoteMedata(int cameraId, int64_t timestamp,
                                            ControlList& metadata) {
    uint32_t size = 0;

    unsigned char* pCur = mMakernoteData.data();
    MEMCPY_S(pCur, mMakernoteData.size(), MAKERNOTE_ID, sizeof(MAKERNOTE_ID));
    pCur += sizeof(MAKERNOTE_ID);

    /**
     * Android JPEG Encode will add APP marker ID and total size to each segment
     * write makerNote content only
     */
    icamera::PlatformData::acquireMakernoteData(cameraId, timestamp, pCur, size);
    if (size > 0) {
        int totalSize = size + sizeof(MAKERNOTE_ID);

        // size of APP0..APP15, set the APP2 segment size only
        std::vector<uint16_t> jpegAppSegmentLength(16, 0);
        jpegAppSegmentLength[2] = totalSize;
        metadata.set(controls::JpegApplicationSegmentLength,
                     Span<const uint16_t, 16>(jpegAppSegmentLength));

        mMakernoteData.resize(totalSize);
        metadata.set(controls::JpegApplicationSegmentContent, mMakernoteData);
    }
}

} /* namespace libcamera */
