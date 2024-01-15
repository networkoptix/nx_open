// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QTimeZone>

#include "filter_params.h"

namespace nx::core::transcoding {

struct TimestampParams
{
    FilterParams filterParams;
    std::chrono::milliseconds timestamp;
    QTimeZone timeZone{QTimeZone::LocalTime};

    bool operator==(const TimestampParams&) const = default;
};
// Serialization is used to store persistent parameters in settings, runtime fields are not needed.
NX_REFLECTION_INSTRUMENT(TimestampParams, (filterParams))

} // namespace nx::core::transcoding
