// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

class SettingsReader;

namespace nx::telemetry {

class NX_TELEMETRY_API Settings
{
public:
    std::string endpoint;

    /**
     * Deployment environment (e.g. prod/staging), exported as resource attribute on telemetry.
     */
    std::string environment;

    /** The maximum buffer/queue size. After the size is reached, spans are dropped. */
    size_t maxQueueSize = 2048;

    /** The maximum batch size of every export. It must be smaller or equal to `maxQueueSize`. */
    size_t maxExportBatchSize = 512;

    /** The time interval between two consecutive exports. */
    std::chrono::milliseconds scheduleDelay = std::chrono::seconds(5);

    void load(const SettingsReader& settings, const QString& prefix = QStringLiteral("telemetry"));
};
NX_REFLECTION_INSTRUMENT(Settings,
    (endpoint)(environment)(maxQueueSize)(maxExportBatchSize)(scheduleDelay))

} // namespace nx::telemetry
