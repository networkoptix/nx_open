// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PluginDiagnosticEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.pluginDiagnostic")

    // TODO: Introduce level type.
    FIELD(int, level, setLevel)

public:
    PluginDiagnosticEvent() = default;

    PluginDiagnosticEvent(
        std::chrono::microseconds timestamp,
        const QString &caption,
        const QString &description,
        QnUuid cameraId,
        QnUuid engineId,
        int level)
        :
        AnalyticsEngineEvent(timestamp, caption, description, cameraId, engineId),
        m_level(level)
    {
    }

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
