// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>

#include <QtCore/QTimeZone>

namespace nx::vms::time {

static constexpr std::chrono::milliseconds kInvalidUtcOffset{std::numeric_limits<qint64>::max()};

/**
 * Calculate timeZone based on its IANA id. Fallback to default value in case of failure.
 */
NX_VMS_COMMON_API QTimeZone timeZone(
    const QString& timeZoneId,
    QTimeZone defaultValue = QTimeZone::LocalTime);

/**
 * Calculate timeZone based on its IANA id. Fallback to offset from UTC in case of failure.
 */
NX_VMS_COMMON_API QTimeZone timeZone(
    const QString& timeZoneId,
    std::chrono::milliseconds offsetFromUtc);

} // namespace nx::vms::time
