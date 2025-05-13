// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_diagnostic_event.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
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
    BasicEvent(timestamp),
    m_deviceId(deviceId),
    m_engineId(engineId),
    m_caption(caption),
    m_description(description),
    m_level(level)
{
}

QString IntegrationDiagnosticEvent::aggregationKey() const
{
    return aggregationResourceId().toSimpleString();
}

QVariantMap IntegrationDiagnosticEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    if (!m_deviceId.isNull())
    {
        fillAggregationDetailsForDevice(result, context, deviceId(), detailLevel);
    }
    else
    {
        const auto engine = context->resourcePool()->getResourceById(engineId());

        const auto engineName = engine ? Strings::resource(engine, detailLevel) : QString();
        result[utils::kAggregationKeyDetailName] = engineName;
        result[utils::kAggregationKeyDetailsDetailName] = Strings::resourceIp(engine);
        result[utils::kAggregationKeyIconDetailName] = "server";

        result[utils::kSourceTextDetailName] = engineName;
        result[utils::kSourceResourcesTypeDetailName] =
            QVariant::fromValue(ResourceType::analyticsEngine);
        result[utils::kSourceResourcesIdsDetailName] = QVariant::fromValue(UuidList{engineId()});
    }

    result[utils::kCaptionDetailName] = m_caption.isEmpty()
        ? manifest().displayName
        : m_caption;
    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, m_description);

    utils::insertLevel(result, calculateLevel(level()));
    utils::insertIcon(result, nx::vms::rules::Icon::integrationDiagnostic);

    result[utils::kDetailingDetailName] = detailing();
    result[utils::kHtmlDetailsName] = QStringList{{
        caption(),
        description()
    }};

    return result;
}

QString IntegrationDiagnosticEvent::extendedCaption(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, aggregationResourceId(), detailLevel);
    return tr("Integration Diagnostic at %1").arg(resourceName);
}

QStringList IntegrationDiagnosticEvent::detailing() const
{
    QStringList details;
    if (!caption().isEmpty())
        details.push_back(Strings::caption(caption()));
    if (!description().isEmpty())
        details.push_back(Strings::description(description()));

    return details;
}

nx::Uuid IntegrationDiagnosticEvent::aggregationResourceId() const
{
    return m_deviceId.isNull() ? m_engineId : m_deviceId;
}

const ItemDescriptor& IntegrationDiagnosticEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<IntegrationDiagnosticEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Integration Diagnostic")),
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
            {utils::kEngineIdFieldName, {ResourceType::analyticsEngine}}}
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
