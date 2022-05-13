// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include "../utils/event_details.h"

namespace nx::vms::rules {

QMap<QString, QString> PluginDiagnosticEvent::details(common::SystemContext* context) const
{
    auto result = AnalyticsEngineEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, tr("Unknown Plugin Diagnostic Event"));

    return result;
}

FilterManifest PluginDiagnosticEvent::filterManifest()
{
    return {};
}

} // namespace nx::vms::rules
