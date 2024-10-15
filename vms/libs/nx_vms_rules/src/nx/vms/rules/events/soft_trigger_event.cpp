// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_event.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/customizable_icon_field.h"
#include "../event_filter_fields/customizable_text_field.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../event_filter_fields/source_user_field.h"
#include "../event_filter_fields/unique_id_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

SoftTriggerEvent::SoftTriggerEvent(
    std::chrono::microseconds timestamp,
    State state,
    nx::Uuid triggerId,
    nx::Uuid cameraId,
    nx::Uuid userId,
    const QString& name,
    const QString& icon)
    :
    BasicEvent(timestamp, state),
    m_triggerId(triggerId),
    m_cameraId(cameraId),
    m_userId(userId),
    m_triggerName(name),
    m_triggerIcon(icon)
{
}

QString SoftTriggerEvent::aggregationSubKey() const
{
    // All the soft trigger events must be considered as unique events.
    return nx::Uuid::createUuid().toString();
}

QString SoftTriggerEvent::resourceKey() const
{
    return m_cameraId.toSimpleString();
}

QString SoftTriggerEvent::aggregationKey() const
{
    // Soft trigger are aggregated regardless to resource.
    // See eventKey in RuleProcessor::processInstantAction.
    return m_triggerId.toSimpleString();
}

QVariantMap SoftTriggerEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    result.insert(utils::kCaptionDetailName, caption());
    result.insert(utils::kExtendedCaptionDetailName, extendedCaption());
    result.insert(utils::kDetailingDetailName, detailing(context));
    utils::insertIfValid(result, utils::kUserIdDetailName, QVariant::fromValue(m_userId));

    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::softTrigger);
    utils::insertIfNotEmpty(result, utils::kCustomIconDetailName, triggerIcon());
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString SoftTriggerEvent::trigger() const
{
    return m_triggerName.isEmpty() ? tr("Trigger Name") : m_triggerName;
}

QString SoftTriggerEvent::caption() const
{
    return QString("%1 %2").arg(manifest().displayName).arg(trigger());
}

QStringList SoftTriggerEvent::detailing(common::SystemContext* context) const
{
    QStringList result;
    result << tr("Source: %1").arg(Strings::resource(context, m_cameraId, Qn::RI_WithUrl));
    result << tr("User: %1").arg(Strings::resource(context, m_userId));
    return result;
}

QString SoftTriggerEvent::extendedCaption() const
{
    return caption();
}

const ItemDescriptor& SoftTriggerEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SoftTriggerEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Soft Trigger")),
        .description = "This event adds a button to one or more devices in the layout. "
            "When clicked, it triggers the associated action either instantly "
            "or continuously (while held). Soft trigger buttons appear "
            "as a circular overlay in the bottom-right corner of the item and display "
            "the <code>triggerName</code> field on hover.",
        .flags = {ItemFlag::instant, ItemFlag::prolonged},
        .fields = {
            makeFieldDescriptor<UniqueIdField>("triggerId", TranslatableString("Invisible")),
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = true,
                    .allowEmptySelection = true
                }.toVariantMap()),
            makeFieldDescriptor<SourceUserField>(
                utils::kUserIdFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("By")),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kUserInputValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<CustomizableTextField>("triggerName",
                NX_DYNAMIC_TRANSLATABLE(tr("Name")),
                "A brief description of the event that will be triggered."),
            makeFieldDescriptor<CustomizableIconField>("triggerIcon",
                NX_DYNAMIC_TRANSLATABLE(tr("Icon")),
                "Icon, displayed within a circular overlay."),
        },
        .resources = {
            {utils::kCameraIdFieldName,
                {ResourceType::device, Qn::ViewContentPermission, Qn::SoftTriggerPermission}},
            {utils::kUserIdFieldName, {ResourceType::user, {}, Qn::WritePermission}}},
        .createPermissions = nx::vms::api::GlobalPermission::none,
        .emailTemplateName = "timestamp_and_details.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
