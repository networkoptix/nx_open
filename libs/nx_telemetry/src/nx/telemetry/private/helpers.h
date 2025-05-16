// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/trace/span.h>

#include "../span.h"

namespace nx::telemetry::helpers {

inline opentelemetry::trace::SpanKind toSpanKind(Span::Kind kind)
{
    using opentelemetry::trace::SpanKind;
    switch (kind)
    {
        case Span::Kind::internal:
            return SpanKind::kInternal;
        case Span::Kind::server:
            return SpanKind::kServer;
        case Span::Kind::client:
            return SpanKind::kClient;
        case Span::Kind::producer:
            return SpanKind::kProducer;
        case Span::Kind::consumer:
            return SpanKind::kConsumer;
    }

    return SpanKind::kInternal;
}

inline opentelemetry::trace::StatusCode toStatusCode(Span::Status status)
{
    using opentelemetry::trace::StatusCode;

    switch (status)
    {
        case Span::Status::empty:
            return StatusCode::kUnset;
        case Span::Status::ok:
            return StatusCode::kOk;
        case Span::Status::error:
            return StatusCode::kError;
    }

    return StatusCode::kUnset;
}

inline opentelemetry::nostd::string_view toStringView(const std::string_view& str)
{
    return opentelemetry::nostd::string_view(str.data(), str.length());
}

inline opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> getTracer()
{
    return opentelemetry::trace::Provider::GetTracerProvider()->GetTracer("telemetry");
}

std::string traceIdString(opentelemetry::trace::TraceId traceId);
std::string spanIdString(opentelemetry::trace::SpanId spanId);

} // namespace nx::telemetry::helpers
