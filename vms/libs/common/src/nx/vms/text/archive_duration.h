#pragma once

#include <QtCore/QCoreApplication>

namespace nx::vms::text {

class ArchiveDuration
{
    Q_DECLARE_TR_FUNCTIONS(ArchiveDuration)
public:
    static QString durationToString(
        const std::chrono::seconds duration, bool isForecastRole = false);
};

} // namespace nx::vms::text
