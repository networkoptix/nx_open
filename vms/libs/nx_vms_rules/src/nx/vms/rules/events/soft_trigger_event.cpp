// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_event.h"

#include "../event_fields/source_camera_field.h"
#include "../event_fields/source_user_field.h"
#include "../event_fields/text_field.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

SoftTriggerEvent::SoftTriggerEvent(
    std::chrono::microseconds timestamp,
    QnUuid cameraId,
    QnUuid userId,
    const QString& triggerName)
    :
    BasicEvent(timestamp),
    m_cameraId(cameraId),
    m_userId(userId),
    m_triggerName(triggerName)
{
}

QString SoftTriggerEvent::uniqueName() const
{
    // All the soft trigger events must be considered as unique events.
    return QnUuid::createUuid().toString();
}

QVariantMap SoftTriggerEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption());
    utils::insertIfValid(result, utils::kSourceIdDetailName, QVariant::fromValue(cameraId()));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kTriggerNameDetailName, trigger());
    utils::insertIfValid(result, utils::kUserIdDetailName, QVariant::fromValue(m_userId));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString SoftTriggerEvent::trigger() const
{
    return m_triggerName.isEmpty() ? tr("Trigger Name") : m_triggerName;
}

QString SoftTriggerEvent::caption() const
{
    return QString("%1 %2").arg(type()).arg(trigger());
}

QString SoftTriggerEvent::detailing() const
{
    return tr("Trigger: %1").arg(trigger());
}

QString SoftTriggerEvent::extendedCaption(common::SystemContext* context) const
{
    return totalEventCount() == 1
        ? tr("Soft Trigger %1 at %2")
            .arg(trigger())
            .arg(utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl))
        : tr("Soft Trigger %1 has been activated multiple times")
            .arg(trigger());
}

const ItemDescriptor& SoftTriggerEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.softTrigger",
        .displayName = tr("Software Trigger"),
        .flags = {ItemFlag::instant, ItemFlag::prolonged},
        .fields = {
            makeFieldDescriptor<SourceCameraField>("cameraId", tr("Camera ID")),
            makeFieldDescriptor<SourceUserField>("userId", tr("User ID")),
            makeFieldDescriptor<EventTextField>("triggerName", tr("Name"))
        },
        .emailTemplatePath = ":/email_templates/software_trigger.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
