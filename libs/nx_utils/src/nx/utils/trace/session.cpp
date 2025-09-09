// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __EMSCRIPTEN__

#include "session.h"

#include <nx/utils/log/log.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace nx::utils::trace {

Session::Session()
{}

std::unique_ptr<Session> Session::start(const QString& fileName)
{
    if (fileName.isEmpty())
    {
        NX_ERROR(NX_SCOPE_TAG, "Trace file name is empty");
        return {};
    }

    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kInProcessBackend;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();

    // The trace config defines which types of data sources are enabled for
    // recording. Currently we just need the "track_event" data source,
    // which corresponds to the TRACE_EVENT trace points.
    perfetto::TraceConfig tracingConfig;
    tracingConfig.add_buffers()->set_size_kb(1024); // 1MB
    auto* config = tracingConfig.add_data_sources()->mutable_config();
    config->set_name("track_event");

    std::unique_ptr<Session> session(new Session());

    session->tracingFile.setFileName(fileName);
    if (!session->tracingFile.open(QIODevice::WriteOnly))
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to open trace file: %1", fileName);
        return {};
    }

    session->tracingSession = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
    session->tracingSession->Setup(tracingConfig, session->tracingFile.handle());
    session->tracingSession->StartBlocking();

    NX_INFO(NX_SCOPE_TAG, "Tracing started: %1", fileName);

    return session;
}

Session::~Session()
{
    // Make sure the last event is closed.
    perfetto::TrackEvent::Flush();

    tracingSession->StopBlocking();
    tracingFile.close();
}

} // nx::utils::trace

#endif
