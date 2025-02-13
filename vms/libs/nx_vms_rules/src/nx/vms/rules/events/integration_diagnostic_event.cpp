// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_diagnostic_event.h"

#include <nx/vms/rules/event_filter_fields/analytics_engine_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_level_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>

#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

namespace {

const auto kCaption =
    NX_DYNAMIC_TRANSLATABLE(IntegrationDiagnosticEvent::tr("Integration Diagnostic Event"));

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

IntegrationDiagnosticEvent::IntegrationDiagnosticEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    nx::Uuid deviceId,
    nx::Uuid engineId,
    nx::vms::api::EventLevel level)
    :
    AnalyticsEngineEvent(timestamp, caption, description, deviceId, engineId),
    m_level(level)
{
}

QVariantMap IntegrationDiagnosticEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = AnalyticsEngineEvent::details(context, aggregatedInfo);

    if (deviceId().isNull())
        result[utils::kSourceIdDetailName] = QVariant::fromValue(engineId());

    if (!result.contains(utils::kCaptionDetailName))
        result.insert(utils::kCaptionDetailName, eventCaption());

    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertLevel(result, calculateLevel(level()));
    utils::insertIcon(result, nx::vms::rules::Icon::integrationDiagnostic);

    return result;
}

QString IntegrationDiagnosticEvent::eventCaption() const
{
    return caption().isEmpty() ? kCaption : caption();
}

QString IntegrationDiagnosticEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, deviceId(), Qn::RI_WithUrl);
    return nx::format("%1 - %2", resourceName, eventCaption());
}

const ItemDescriptor& IntegrationDiagnosticEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<IntegrationDiagnosticEvent>(),
        .displayName = kCaption,
        .description = "Triggered when an event is received from "
            "a plugin device connected to the site.",
        .fields = {
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = true,
                    .allowEmptySelection = true
                }.toVariantMap()),
            makeFieldDescriptor<AnalyticsEngineField>("engineId",
                NX_DYNAMIC_TRANSLATABLE(tr("For Plugin"))),
            makeFieldDescriptor<TextLookupField>(utils::kCaptionFieldName,
                Strings::andCaption(),
                "An optional value used to specify the object type identifier."),
            makeFieldDescriptor<TextLookupField>(utils::kDescriptionFieldName,
                Strings::andDescription(),
                "An optional attribute used to differentiate objects within "
                "the same class for event filtering."),
            makeFieldDescriptor<AnalyticsEventLevelField>("level",
                NX_DYNAMIC_TRANSLATABLE(tr("And Level Is"))),
        },
        .resources = {
            {utils::kDeviceIdFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kEngineIdFieldName, {ResourceType::analyticsEngine}}},
        .emailTemplateName = "integration_diagnostic.mustache"
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
