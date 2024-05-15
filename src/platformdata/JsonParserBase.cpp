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

#define LOG_TAG JsonParserBase

#include "JsonParserBase.h"

#include <string>
#include <fstream>

#include "iutils/CameraLog.h"

namespace icamera {

Json::Value JsonParserBase::openJsonFile(const std::string& filename) {
    Json::Value root;
    std::ifstream ifs;
    ifs.open(filename);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    JSONCPP_STRING errs;

    if (!parseFromStream(builder, ifs, &root, &errs))
        LOGE("%s: Cannot load json file %s for %s", __func__, filename.c_str(), errs.c_str());

    return root;
}

}  // namespace icamera
