// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/analytics_attributes_field.h"
#include "../event_filter_fields/analytics_event_type_field.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../event_filter_fields/text_lookup_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsEvent::AnalyticsEvent(
    std::chrono::microseconds timestamp,
    State state,
    const QString& caption,
    const QString& description,
    nx::Uuid deviceId,
    nx::Uuid engineId,
    const QString& eventTypeId,
    const nx::common::metadata::Attributes& attributes,
    nx::Uuid objectTrackId,
    const QString& key,
    const QRectF& boundingBox)
    :
    AnalyticsEngineEvent(timestamp, caption, description, deviceId, engineId),
    m_eventTypeId(eventTypeId),
    m_attributes(attributes),
    m_objectTrackId(objectTrackId),
    m_key(key)
{
    setState(state);
    if (boundingBox.isValid())
        setBoundingBox(boundingBox);
}

QString AnalyticsEvent::subtype() const
{
    return eventTypeId();
}

QString AnalyticsEvent::resourceKey() const
{
    return utils::makeKey(
        AnalyticsEngineEvent::resourceKey(),
        m_eventTypeId,
        m_objectTrackId.toSimpleString(),
        m_key);
}

QString AnalyticsEvent::aggregationKey() const
{
    return deviceId().toSimpleString();
}

QVariantMap AnalyticsEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = AnalyticsEngineEvent::details(context, aggregatedInfo);

    if (!result.contains(utils::kCaptionDetailName))
        result.insert(utils::kCaptionDetailName, analyticsEventCaption(context));

    utils::insertIfNotEmpty(result, utils::kExtraCaptionDetailName, extraCaption());

    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));

    utils::insertIfNotEmpty(
        result, utils::kAnalyticsEventTypeDetailName, analyticsEventCaption(context));
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::analyticsEvent);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString AnalyticsEvent::analyticsEventCaption(common::SystemContext* context) const
{
    auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId());

    const auto eventType = camera && camera->systemContext()
        ? camera->systemContext()->analyticsTaxonomyState()->eventTypeById(m_eventTypeId)
        : nullptr;

    return eventType ? eventType->name() : tr("Analytics Event");
}

QString AnalyticsEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, deviceId(), Qn::RI_WithUrl);
    const auto eventCaption = analyticsEventCaption(context);

    return tr("%1 at %2", "Analytics Event at some camera")
        .arg(eventCaption)
        .arg(resourceName);
}

const ItemDescriptor& AnalyticsEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<AnalyticsEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Analytics Event")),
        .description = TranslatableString{"Triggered when an analytics event is triggered"
            " on source device."},
        .flags = {ItemFlag::instant, ItemFlag::prolonged}, //< Actual duration depends on the AnalyticsEventTypeField.
        .fields = {
            utils::makeStateFieldDescriptor(Strings::beginWhen()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraAnalyticsEventsValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<AnalyticsEventTypeField>(
                utils::kEventTypeIdFieldName,
                Strings::ofType(),
                "Specifies the analytics event types used for event filtering.",
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TextLookupField>(
                utils::kCaptionFieldName,
                Strings::andCaption(),
                "An optional value used to specify the object type identifier."),
            makeFieldDescriptor<TextLookupField>(
                utils::kDescriptionFieldName,
                Strings::andDescription(),
                "An optional attribute used to differentiate objects within "
                "the same class for event filtering."),
            makeFieldDescriptor<AnalyticsAttributesField>(
                utils::kAttributesFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("And attributes")),
                "An optional text for matching event attributes."),
        },
        .resources = {
            {utils::kDeviceIdFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kEngineIdFieldName, {ResourceType::analyticsEngine}}},
        .emailTemplateName = "analytics_event.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
