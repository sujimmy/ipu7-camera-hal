/*
 * Copyright (C) 2020-2022 Intel Corporation.
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

#define LOG_TAG ShareRefer

#include "ShareReferBufferPool.h"

#include "PlatformData.h"
#include "iutils/CameraLog.h"
#include "iutils/Errors.h"

using std::map;
using std::unique_ptr;
using std::vector;

namespace icamera {

#define CONSUMER_BUFFER_NUM 2

int64_t ShareReferBufferPool::constructReferId(int32_t streamId, int32_t pgId, int32_t portId) {
    return (((int64_t)streamId << 32) + ((int64_t)pgId << 16) + portId);
}

ShareReferBufferPool::~ShareReferBufferPool() {
    AutoMutex l(mPairLock);
    while (!mUserPairs.empty()) {
        UserPair* pair = mUserPairs.back();
        mUserPairs.pop_back();
        delete pair;
    }
}

int32_t ShareReferBufferPool::setReferPair(const std::string& producerPgName, int64_t producerId,
                                           const std::string& consumerPgName, int64_t consumerId) {
    CheckAndLogError(producerId == consumerId, BAD_VALUE, "same pair for producer/consumer %lx",
                     producerId);

    UserPair* pair = new UserPair;
    pair->producerPgName = producerPgName;
    pair->producerId = producerId;
    pair->consumerPgName = consumerPgName;
    pair->consumerId = consumerId;
    pair->busy = false;
    pair->active = true;
    LOG1("%s: %s:%lx -> %s:%lx", __func__, producerPgName.c_str(), producerId,
         consumerPgName.c_str(), consumerId);
    AutoMutex l(mPairLock);
    mUserPairs.push_back(pair);
    return OK;
}

int32_t ShareReferBufferPool::clearReferPair(int64_t id) {
    AutoMutex l(mPairLock);
    for (auto it = mUserPairs.begin(); it != mUserPairs.end(); it++) {
        UserPair* pair = *it;
        if (pair->producerId != id && pair->consumerId != id) continue;

        pair->bufferLock.lock();
        if (pair->busy) {
            pair->bufferLock.unlock();
            LOGE("Can't clear pair %lx because Q is busy!", id);
            return UNKNOWN_ERROR;
        }

        mUserPairs.erase(it);
        pair->bufferLock.unlock();
        delete pair;
        return OK;
    }

    return BAD_VALUE;
}

int32_t ShareReferBufferPool::getMinBufferNum(int64_t id) {
    AutoMutex l(mPairLock);
    for (auto pair : mUserPairs) {
        if (pair->producerId == id)
            return PlatformData::getMaxRawDataNum(mCameraId);
        else if (pair->consumerId == id)
            return CONSUMER_BUFFER_NUM;
    }
    return 0;
}

ShareReferBufferPool::UserPair* ShareReferBufferPool::findUserPair(int64_t id) {
    for (auto pair : mUserPairs) {
        if (pair->consumerId == id || pair->producerId == id) {
            return pair;
        }
    }
    return nullptr;
}

}  // namespace icamera
