// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

namespace nx::vms::text {

class NX_VMS_COMMON_API ArchiveDuration
{
    Q_DECLARE_TR_FUNCTIONS(ArchiveDuration)
public:
    static QString durationToString(
        const std::chrono::seconds duration, bool isForecastRole = false);
};

} // namespace nx::vms::text
