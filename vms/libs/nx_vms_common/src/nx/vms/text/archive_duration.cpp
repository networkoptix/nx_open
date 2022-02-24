// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "archive_duration.h"
#include "human_readable.h"

namespace nx::vms::text {

QString ArchiveDuration::durationToString(const std::chrono::seconds duration, bool isForecastRole)
{
    const qreal kSecondsPerHour = 3600.0;

    if (duration.count() == 0)
        return isForecastRole ? tr("no data for forecast") : tr("empty");

    if (duration.count() < kSecondsPerHour)
        return tr("less than an hour");

    static const QString kSeparator(' ');

    return HumanReadable::timeSpan(duration,
        HumanReadable::Years | HumanReadable::Months | HumanReadable::Days | HumanReadable::Hours,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);
}

} // namespace nx::vms::text
