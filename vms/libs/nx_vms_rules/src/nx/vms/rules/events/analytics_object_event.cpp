// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/analytics_object_type_field.h"
#include "../event_filter_fields/object_lookup_field.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsObjectEvent::AnalyticsObjectEvent(
    State state,
    std::chrono::microseconds timestamp,
    nx::Uuid cameraId,
    nx::Uuid engineId,
    const QString& objectTypeId,
    nx::Uuid objectTrackId,
    const nx::common::metadata::Attributes& attributes)
    :
    AnalyticsEngineEvent(
        timestamp, /*caption*/ QString(), /*description*/ QString(), cameraId, engineId),
    m_objectTypeId(objectTypeId),
    m_objectTrackId(objectTrackId),
    m_attributes(attributes)
{
    setState(state);
}

QString AnalyticsObjectEvent::subtype() const
{
    return objectTypeId();
}

QString AnalyticsObjectEvent::resourceKey() const
{
    return utils::makeKey(AnalyticsEngineEvent::resourceKey(), m_objectTrackId.toSimpleString());
}

QString AnalyticsObjectEvent::aggregationKey() const
{
    return cameraId().toSimpleString();
}

QString AnalyticsObjectEvent::cacheKey() const
{
    return state() == State::started ? m_objectTrackId.toSimpleString() : QString();
}

nx::analytics::taxonomy::AbstractObjectType* AnalyticsObjectEvent::objectTypeById(
    common::SystemContext* context) const
{
    const auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId());

    return camera && camera->systemContext()
        ? camera->systemContext()->analyticsTaxonomyState()->objectTypeById(m_objectTypeId)
        : nullptr;
}

QVariantMap AnalyticsObjectEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = AnalyticsEngineEvent::details(context, aggregatedInfo);

    result.insert(utils::kCaptionDetailName, analyticsObjectCaption(context));
    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));

    // TODO: #sivanov Do we need to repeat this as a separate property?
    result.insert(utils::kAnalyticsObjectTypeDetailName, analyticsObjectCaption(context));

    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::analyticsObjectDetected);

    const auto objectType = objectTypeById(context);
    utils::insertIfNotEmpty(
        result, utils::kCustomIconDetailName, objectType ? objectType->icon() : QString());
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString AnalyticsObjectEvent::analyticsObjectCaption(common::SystemContext* context) const
{
    const auto objectType = objectTypeById(context);
    return objectType ? objectType->name() : tr("Object detected");
}

QString AnalyticsObjectEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, cameraId(), Qn::RI_WithUrl);
    const auto objectCaption = analyticsObjectCaption(context);

    return tr("%1 at %2", " is detected")
        .arg(objectCaption)
        .arg(resourceName);

    return BasicEvent::extendedCaption(context);
}

const ItemDescriptor& AnalyticsObjectEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<AnalyticsObjectEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Analytics Object Detected")),
        .description = "Triggered when an analytics object is detected on source device. "
            "This event is specific to video analytics that provide object detection metadata, "
            "enabling accurate categorization based on the object type",
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::state()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraAnalyticsValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<AnalyticsObjectTypeField>(
                utils::kObjectTypeIdFieldName,
                Strings::ofType(),
                {},
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<ObjectLookupField>(
                utils::kAttributesFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("And Object"))),
        },
        .resources = {
            {utils::kCameraIdFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kEngineIdFieldName, {ResourceType::analyticsEngine}}},
        .emailTemplateName = "analytics_object.mustache"
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
