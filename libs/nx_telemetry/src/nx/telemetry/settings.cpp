// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::telemetry {

void Settings::load(const SettingsReader& settings, const QString& prefix)
{
    QnSettingsGroupReader group(settings, prefix);
    endpoint = group.value("endpoint").toString().toStdString();
    environment = group.value("environment").toString().toStdString();
    maxQueueSize = group.value("maxQueueSize", QVariant::fromValue(maxQueueSize)).toULongLong();
    maxExportBatchSize =
        group.value("maxExportBatchSize", QVariant::fromValue(maxExportBatchSize)).toULongLong();
    scheduleDelay = std::chrono::milliseconds(
        group.value("scheduleDelayMs", QVariant::fromValue(scheduleDelay.count())).toULongLong());
}

} // namespace nx::telemetry
