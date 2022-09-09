// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/vms/common/system_context.h>

#include "../event_fields/source_camera_field.h"
#include "../event_fields/analytics_event_type_field.h"
#include "../event_fields/analytics_object_attributes_field.h"
#include "../event_fields/keywords_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsEvent::AnalyticsEvent(
    std::chrono::microseconds timestamp,
    State state,
    const QString& caption,
    const QString& description,
    QnUuid cameraId,
    QnUuid engineId,
    const QString& eventTypeId,
    const nx::common::metadata::Attributes& attributes,
    QnUuid objectTrackId,
    const QString& key)
    :
    AnalyticsEngineEvent(timestamp, caption, description, cameraId, engineId),
    m_eventTypeId(eventTypeId),
    m_attributes(attributes),
    m_objectTrackId(objectTrackId),
    m_key(key)
{
    setState(state);
}

QString AnalyticsEvent::resourceKey() const
{
    return makeName(
        AnalyticsEngineEvent::resourceKey(),
        m_eventTypeId,
        m_objectTrackId.toSimpleString(),
        m_key);
}

QString AnalyticsEvent::aggregationKey() const
{
    return cameraId().toSimpleString();
}

QVariantMap AnalyticsEvent::details(common::SystemContext* context) const
{
    auto result = AnalyticsEngineEvent::details(context);

    if (!result.contains(utils::kCaptionDetailName))
        utils::insertIfNotEmpty(result, utils::kCaptionDetailName, analyticsEventCaption(context));

    utils::insertIfNotEmpty(result, utils::kExtraCaptionDetailName, extraCaption());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kHasScreenshotDetailName, true);
    utils::insertIfNotEmpty(result, utils::kAnalyticsEventTypeDetailName, analyticsEventCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::resource);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString AnalyticsEvent::analyticsEventCaption(common::SystemContext* context) const
{
    auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId());

    const auto eventType = camera && camera->systemContext()
        ? camera->systemContext()->analyticsTaxonomyState()->eventTypeById(m_eventTypeId)
        : nullptr;

    return eventType ? eventType->name() : tr("Analytics Event");
}

QString AnalyticsEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl);
    const auto eventCaption = analyticsEventCaption(context);

    return tr("%1 at %2", "Analytics Event at some camera")
        .arg(eventCaption)
        .arg(resourceName);
}

const ItemDescriptor& AnalyticsEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.analytics",
        .displayName = tr("Analytics Event"),
        .description = "",
        .flags = {ItemFlag::instant, ItemFlag::prolonged},
        .fields = {
            utils::makeStateFieldDescriptor(tr("State")),
            makeFieldDescriptor<SourceCameraField>(utils::kCameraIdFieldName, tr("Camera")),
            makeFieldDescriptor<AnalyticsEventTypeField>("eventTypeId", tr("Event Type")),
            makeFieldDescriptor<KeywordsField>("caption", tr("Caption")),
            makeFieldDescriptor<KeywordsField>("description", tr("Description")),
            // TODO: #amalov Consider adding following fields in 5.1+.
            // makeFieldDescriptor<AnalyticsObjectAttributesField>("attributes", tr("Attributes")),
        },
        .permissions = {
            .resourcePermissions = {
                {utils::kCameraIdFieldName, {Qn::ViewContentPermission, Qn::ViewFootagePermission}}
            }
        },
        .emailTemplatePath = ":/email_templates/analytics_event.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
