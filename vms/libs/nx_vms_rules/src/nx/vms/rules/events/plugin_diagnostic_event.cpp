// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/vms/rules/event_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_fields/analytics_event_level_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>

#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

namespace {

nx::vms::event::Level calculateLevel(nx::vms::api::EventLevel eventLevel)
{
    using nx::vms::api::EventLevel;
    using nx::vms::event::Level;

    switch (eventLevel)
    {
        case EventLevel::ErrorEventLevel:
            return Level::critical;

        case EventLevel::WarningEventLevel:
            return Level::important;

        default:
            return Level::common;
    }
}

Icon calculateIcon(nx::vms::api::EventLevel eventLevel)
{
    using nx::vms::api::EventLevel;

    switch (eventLevel)
    {
        case EventLevel::ErrorEventLevel:
            return Icon::critical;

        case EventLevel::WarningEventLevel:
            return Icon::important;

        default:
            return Icon::alert;
    }
}

} // namespace

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
    utils::insertLevel(result, calculateLevel(level()));
    utils::insertIcon(result, calculateIcon(level()));

    return result;
}

QString PluginDiagnosticEvent::eventCaption() const
{
    return caption().isEmpty() ? tr("Plugin Diagnostic Event") : caption();
}

QString PluginDiagnosticEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl);
    return nx::format("%1 - %2", resourceName, eventCaption());
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
        },
        .permissions = {
            .resourcePermissions = {{utils::kCameraIdFieldName, Qn::ViewContentPermission}}
        },
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
