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
    nx::Uuid deviceId,
    nx::Uuid userId,
    const QString& name,
    const QString& icon)
    :
    BasicEvent(timestamp, state),
    m_triggerId(triggerId),
    m_deviceId(deviceId),
    m_userId(userId),
    m_triggerName(name),
    m_triggerIcon(icon)
{
}

QVariantMap SoftTriggerEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);

    utils::insertIfValid(result, utils::kUserIdDetailName, QVariant::fromValue(m_userId));

    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::softTrigger);
    utils::insertIfNotEmpty(result, utils::kCustomIconDetailName, triggerIcon());
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    result[utils::kAggregationKeyDetailName] = Strings::softTriggerName(m_triggerName);
    result[utils::kAggregationKeyIconDetailName] = "soft_trigger";

    result[utils::kSourceTextDetailName] = Strings::resource(context, deviceId(), detailLevel);
    result[utils::kSourceResourcesTypeDetailName] = QVariant::fromValue(ResourceType::device);
    result[utils::kSourceResourcesIdsDetailName] = QVariant::fromValue(UuidList{deviceId()});

    result[utils::kCaptionDetailName] = m_triggerName;

    const QString user = Strings::resource(context, m_userId);
    const QString userItem = tr("User: %1").arg(user);
    result[utils::kDescriptionDetailName] = userItem;

    const QString source = Strings::resource(context, deviceId(), detailLevel);
    const QString sourceItem = Strings::source(source);

    result[utils::kDetailingDetailName] = QStringList{{sourceItem, userItem}};
    result[utils::kHtmlDetailsName] = QStringList{{source, user}};

    return result;
}

QString SoftTriggerEvent::extendedCaption(common::SystemContext* /*context*/,
    Qn::ResourceInfoLevel /*detailLevel*/) const
{
    return nx::format("%1 %2", manifest().displayName, m_triggerName);
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
            utils::makeStateFieldDescriptor(
                Strings::state(), /*description*/ {}, State::instant, /*visible*/ false),
            makeFieldDescriptor<UniqueIdField>("triggerId", TranslatableString("Invisible")),
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
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
            {utils::kDeviceIdFieldName,
                {ResourceType::device, Qn::ViewContentPermission, Qn::SoftTriggerPermission}},
            {utils::kUserIdFieldName, {ResourceType::user, {}, Qn::WritePermission}}},
        .createPermissions = nx::vms::api::GlobalPermission::none
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
