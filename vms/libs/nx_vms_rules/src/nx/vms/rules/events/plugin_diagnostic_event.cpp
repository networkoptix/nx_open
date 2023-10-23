// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_level_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>

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
        case EventLevel::error:
            return Level::critical;

        case EventLevel::warning:
            return Level::important;

        default:
            return Level::common;
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
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                tr("Occurs at"),
                {},
                {{"acceptAll", true}}),
            makeFieldDescriptor<AnalyticsEngineField>(
                "engineId", tr("For Plugin"), {}, {}, {utils::kCameraIdFieldName}),
            makeFieldDescriptor<TextLookupField>(utils::kCaptionFieldName, tr("And Caption")),
            makeFieldDescriptor<TextLookupField>(utils::kDescriptionFieldName, tr("And Description")),
            makeFieldDescriptor<AnalyticsEventLevelField>("level", tr("And Level Is")),
        },
        .permissions = {
            .resourcePermissions = {{utils::kCameraIdFieldName, Qn::ViewContentPermission}}
        },
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
