// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_queue_size.h"

namespace {

static constexpr int kMaxDurationPartsCount = 2;

} // namespace

namespace nx::vms::client::desktop {

bool BackupQueueSize::isEmpty() const
{
    return duration.count() <= 0;
}

QString BackupQueueSize::toString() const
{
    if (isEmpty())
        return QString();

    auto seconds = duration;

    QStringList durationParts;
    const auto joinDurationParts =
        [&durationParts]
        {
            return durationParts.mid(0, kMaxDurationPartsCount).join(QChar::Space);
        };

    if (const auto weeks = std::chrono::duration_cast<std::chrono::weeks>(seconds);
        weeks.count() > 0)
    {
        durationParts.push_back(tr("%n weeks", "", weeks.count()));
        seconds -= weeks;
    }

    if (const auto days = std::chrono::duration_cast<std::chrono::days>(seconds);
        days.count() > 0)
    {
        durationParts.push_back(tr("%n days", "", days.count()));
        seconds -= days;
    }
    else if (!durationParts.isEmpty())
    {
        return joinDurationParts();
    }

    if (const auto hours = std::chrono::duration_cast<std::chrono::hours>(seconds);
        hours.count() > 0)
    {
        durationParts.push_back(tr("%n hours", "", hours.count()));
        seconds -= hours;
    }
    else if (!durationParts.isEmpty())
    {
        return joinDurationParts();
    }

    if (const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(seconds);
        minutes.count() > 0)
    {
        durationParts.push_back(tr("%n minutes", "", minutes.count()));
        seconds -= minutes;
    }
    else if (!durationParts.isEmpty())
    {
        return joinDurationParts();
    }

    if (seconds.count() > 0)
        durationParts.push_back(tr("%n seconds", "", seconds.count()));

    return joinDurationParts();
}

} // namespace nx::vms::client::desktop
