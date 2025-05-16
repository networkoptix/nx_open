// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <opentelemetry/trace/span.h>

#include "../span.h"

namespace nx::telemetry {

struct SpanData
{
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    bool reference = false;

    // Debug data.
    Span::Kind kind = Span::Kind::internal;
    std::string name;
    std::string parentId;

    SpanData()
    {
    }

    SpanData(
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
        bool reference = false)
        :
        span(span),
        reference(reference)
    {
    }

    ~SpanData()
    {
        if (span && !reference)
            span->End();
    }
};

} // namespace nx::telemetry
