// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/vms/rules/event_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_fields/analytics_event_level_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>

#include "../utils/event_details.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

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

QVariantMap PluginDiagnosticEvent::details(common::SystemContext* context) const
{
    auto result = AnalyticsEngineEvent::details(context);

    if (!result.contains(utils::kCaptionDetailName))
        result.insert(utils::kCaptionDetailName, eventCaption());

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));

    return result;
}

QString PluginDiagnosticEvent::eventCaption() const
{
    return caption().isEmpty() ? tr("Plugin Diagnostic Event") : caption();
}

QString PluginDiagnosticEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl);
    return tr("%1 - %2").arg(resourceName).arg(eventCaption());
}

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

} // namespace nx::vms::rules
