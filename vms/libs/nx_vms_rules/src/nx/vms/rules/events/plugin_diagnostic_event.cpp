// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/vms/rules/event_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_fields/analytics_event_level_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>

#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PluginDiagnosticEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PluginDiagnosticEvent>(),
        .displayName = tr("Plugin Diagnostic Event"),
        .description = {},
        .fields = {
            makeFieldDescriptor<SourceCameraField>("cameraId", tr("Camera")),
            makeFieldDescriptor<AnalyticsEngineField>("engineId", tr("Source")),
            makeFieldDescriptor<KeywordsField>("caption", tr("Caption")),
            makeFieldDescriptor<KeywordsField>("description", tr("Description")),
            makeFieldDescriptor<AnalyticsEventLevelField>("level", tr("Level")),
        }
    };

    return kDescriptor;
}

PluginDiagnosticEvent::PluginDiagnosticEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    QnUuid cameraId,
    QnUuid engineId,
    nx::vms::api::EventLevel level)
    :
    AnalyticsEngineEvent(timestamp, caption, description, cameraId, engineId),
    m_level(level)
{
}

QMap<QString, QString> PluginDiagnosticEvent::details(common::SystemContext* context) const
{
    auto result = AnalyticsEngineEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, tr("Unknown Plugin Diagnostic Event"));

    return result;
}

} // namespace nx::vms::rules
