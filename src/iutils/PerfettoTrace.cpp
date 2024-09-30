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

#include "utils/PerfettoTrace.h"

#include <cstdlib>
#include <mutex>
#include <thread>

namespace icamera {
bool gPerfettoEnabled = false;
}

class PerfettoTrace : public perfetto::TrackEventSessionObserver {
 public:
    PerfettoTrace() { perfetto::TrackEvent::AddSessionObserver(this); }
    ~PerfettoTrace() override { perfetto::TrackEvent::RemoveSessionObserver(this); }

    void OnStart(const perfetto::DataSourceBase::StartArgs&) override {
        icamera::gPerfettoEnabled = true;
    }

    void OnStop(const perfetto::DataSourceBase::StopArgs&) override {
        icamera::gPerfettoEnabled = false;
    }
};

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

static PerfettoTrace* gPerfettoAgent = nullptr;
static std::once_flag gPerfettoOnce;

static void uninitPerfettoTrace() {
    ::perfetto::TrackEvent::Flush();
    icamera::gPerfettoEnabled = false;
    delete gPerfettoAgent;
}

void initPerfettoTrace() {
    std::call_once(gPerfettoOnce, [&]() {
        perfetto::TracingInitArgs args;
        args.backends = perfetto::kSystemBackend;

        perfetto::Tracing::Initialize(args);
        perfetto::TrackEvent::Register();

        gPerfettoAgent = new PerfettoTrace;
        ::atexit(::uninitPerfettoTrace);
    });
}

ScopedPerfetto::ScopedPerfetto(const char* traceInfo, const char* note1, int value1,
                               const char* note2, int value2, const char* note3, int value3)
        : mEnableAtraceEnd(false) {
    if (!icamera::gPerfettoEnabled) return;
    mEnableAtraceEnd = true;
    if (note1 == nullptr) {
        TRACE_EVENT_BEGIN(PERFETTO_CATEGORIES, perfetto::StaticString(traceInfo));
    } else if (note2 == nullptr) {
        TRACE_EVENT_BEGIN(PERFETTO_CATEGORIES, perfetto::StaticString(traceInfo), note1, value1);
    } else if (note3 == nullptr) {
        TRACE_EVENT_BEGIN(PERFETTO_CATEGORIES, perfetto::StaticString(traceInfo), note1, value1,
                          note2, value2);
    } else {
        TRACE_EVENT_BEGIN(PERFETTO_CATEGORIES, perfetto::StaticString(traceInfo), note1, value1,
                          note2, value2, note3, value3);
    }
}

ScopedPerfetto::~ScopedPerfetto() {
    if (mEnableAtraceEnd) {
        TRACE_EVENT_END(PERFETTO_CATEGORIES);
    }
}
