// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PluginDiagnosticEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.pluginDiagnostic")

    FIELD(nx::vms::api::EventLevel, level, setLevel)

public:
    static const ItemDescriptor& manifest();

    PluginDiagnosticEvent() = default;

    PluginDiagnosticEvent(
        std::chrono::microseconds timestamp,
        const QString &caption,
        const QString &description,
        QnUuid cameraId,
        QnUuid engineId,
        nx::vms::api::EventLevel level);

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

};

} // namespace nx::vms::rules
