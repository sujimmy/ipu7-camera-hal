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

#include "IPAServerThread.h"

#include <libcamera/base/log.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

namespace ipa::ipu7 {

IPAServerThread::IPAServerThread(IAlgoWorker* worker, const char* name)
    : mIAlgoWorker(worker),
      mName(name),
      mState(State::Stopped) {
    LOG(IPAIPU, Debug) << " " << __func__ << " " << mName.c_str();

    start();

    LOG(IPAIPU, Debug) << " " << __func__ << " started " << mName.c_str();
    {
        MutexLocker locker(mMutex);
        mState = State::Running;
    }
}

IPAServerThread::~IPAServerThread() {
    LOG(IPAIPU, Debug) << " " << __func__ << " " << mName.c_str();
    {
        MutexLocker locker(mMutex);
        mState = State::Stopped;
        mEventCondition.notify_one();
    }

    wait();

    LOG(IPAIPU, Debug) << " " << __func__ << " stopped " << mName.c_str();
}

void IPAServerThread::sendRequest(uint32_t cmd, const Span<uint8_t>& mem) {
    MutexLocker locker(mMutex);
    LOG(IPAIPU, Debug) << "sendRequest cmd " << cmd << " " << mName.c_str() << " this " << this;

    cmd_event event = {cmd, mem.data(), static_cast<int>(mem.size())};
    mEventQueue.push(std::move(event));
    mEventCondition.notify_one();
}

void IPAServerThread::run() {
    LOG(IPAIPU, Debug) << " " << __func__ << " run thread " << mName.c_str();
    while (true) {
        cmd_event event;
        {
            MutexLocker locker(mMutex);

            auto isEmpty = ([&]() LIBCAMERA_TSA_REQUIRES(mMutex) {
                return mState != State::Running || !mEventQueue.empty();
            });

            if (mState != State::Running) break;

            auto duration = std::chrono::duration<float, std::ratio<1 / 1>>(5);
            if (mEventQueue.empty()) {
                mEventCondition.wait_for(locker, duration, isEmpty);
            }

            if (mEventQueue.empty()) continue;

            event = std::move(mEventQueue.front());
            mEventQueue.pop();
        }

        mIAlgoWorker->handleEvent(event);
    }
}

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
