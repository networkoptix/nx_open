// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

namespace nx::telemetry::helpers {

using opentelemetry::trace::TraceId;
using opentelemetry::trace::SpanId;

std::string traceIdString(TraceId traceId)
{
    constexpr int kSize = TraceId::kSize * 2;

    std::string result(kSize, '\0');
    traceId.ToLowerBase16(opentelemetry::nostd::span<char, kSize>(result.data(), kSize));
    return result;
}

std::string spanIdString(SpanId spanId)
{
    constexpr int kSize = SpanId::kSize * 2;

    std::string result(kSize, '\0');
    spanId.ToLowerBase16(opentelemetry::nostd::span<char, kSize>(result.data(), kSize));
    return result;
}

} // namespace nx::telemetry::helpers
