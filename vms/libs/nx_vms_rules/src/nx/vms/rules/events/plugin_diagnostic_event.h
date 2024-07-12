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
        nx::Uuid cameraId,
        nx::Uuid engineId,
        nx::vms::api::EventLevel level);

    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

private:
    QString eventCaption() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
