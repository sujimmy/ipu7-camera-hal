/*
 * Copyright (C) 2022-2024 Intel Corporation
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

#include "ISchedulerNode.h"
#include "BufferQueue.h"

namespace icamera {

struct StageControl {
    bool stillTnrReferIn;  // Fake task to generate still tnr refer-in frame for still tnr

    StageControl() : stillTnrReferIn(false) {}
};

/**
 * \Interface IPipeStage
 */
class IPipeStage : public ISchedulerNode, public BufferQueue {
 public:
    explicit IPipeStage(const char* name, int stageId) : ISchedulerNode(name), mId(stageId) {}
    virtual ~IPipeStage() {}

    virtual bool process(int64_t triggerId) = 0;

    int getId() const { return mId; }
    virtual int start() = 0;
    virtual int stop() = 0;

    virtual void setControl(int64_t sequence, const StageControl& control) = 0;

    virtual void setInputTerminals(const std::map<uuid, uint32_t>& inputPortTerminals) {
        mInputPortTerminals = inputPortTerminals;
    }

 private:
    int mId;

 protected:
    // Consumer's input ports are decided by output ports of producer,
    // So it needs to map input ports to input terminals of itself.
    // <input port (producer output port), consumer input terminals>
    std::map<uuid, uint32_t> mInputPortTerminals;
};

}  // namespace icamera
