// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timezone.h"

namespace nx::vms::time {

QTimeZone timeZone(const QString& timeZoneId, QTimeZone defaultValue)
{
    if (timeZoneId.isEmpty())
        return defaultValue;

    const QByteArray rawTimeZoneId = timeZoneId.toUtf8();
    const bool isTzAvailable = QTimeZone::isTimeZoneIdAvailable(rawTimeZoneId);
    if (!isTzAvailable)
        return defaultValue;

    QTimeZone result(rawTimeZoneId);
    return result.isValid()
        ? result
        : defaultValue;
}

QTimeZone timeZone(const QString& timeZoneId, std::chrono::milliseconds offsetFromUtc)
{
    if (offsetFromUtc == kInvalidUtcOffset)
        return timeZone(timeZoneId, /*defaultValue*/ QTimeZone::LocalTime);

    const auto utcOffset = duration_cast<std::chrono::seconds>(offsetFromUtc);
    return timeZone(timeZoneId, /*defaultValue*/ QTimeZone::fromDurationAheadOfUtc(utcOffset));
}

} // namespace nx::vms::time
