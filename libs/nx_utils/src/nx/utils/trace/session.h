// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifndef __EMSCRIPTEN__

#include <QtCore/QFile>

#include "trace_categories.h"

namespace nx::utils::trace {

class NX_UTILS_API Session final
{
    Session();

public:
    ~Session();

    static std::unique_ptr<Session> start(const QString& fileName);

private:
    QFile tracingFile;
    std::unique_ptr<perfetto::TracingSession> tracingSession;
};

} // nx::utils::trace

#endif
