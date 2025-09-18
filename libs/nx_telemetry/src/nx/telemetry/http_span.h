// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/std_string_utils.h>

#include "span.h"

using HttpHeaders = std::multimap<std::string, std::string, nx::utils::ci_less>;

namespace nx::telemetry {

class NX_TELEMETRY_API HttpSpan: public Span
{
    using Span::Span;

public:
    HttpSpan(const Span& span);

    void setStatusCode(int httpStatusCode);
    void setRoute(std::string_view route);

    static HttpSpan clientSpan(
        std::string_view method,
        std::string_view url,
        HttpHeaders& headers);

    static HttpSpan serverSpan(
        std::string_view method,
        std::string_view route,
        std::string_view url,
        const HttpHeaders& headers);

    static Span extractSpanFromHeaders(const HttpHeaders& headers);
};

} // namespace nx::telemetry
