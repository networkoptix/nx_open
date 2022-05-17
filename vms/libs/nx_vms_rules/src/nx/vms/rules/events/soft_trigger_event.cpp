// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_event.h"

#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_fields/source_camera_field.h"
#include "../event_fields/source_user_field.h"
#include "../event_fields/text_field.h"
#include "../utils/event_details.h"

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

QMap<QString, QString> SoftTriggerEvent::details(common::SystemContext* context) const
{
    QMap<QString, QString> result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption());
    utils::insertIfNotEmpty(result, utils::kSourceDetailName, source(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString SoftTriggerEvent::caption() const
{
    return QString("%1 %2").arg(type()).arg(triggerName());
}

QString SoftTriggerEvent::source(common::SystemContext* context) const
{
    auto cameraResource = context->resourcePool()->getResourceById(cameraId());
    if (!cameraResource)
        return {};

    constexpr auto detailLevel = Qn::ResourceInfoLevel::RI_NameOnly; // TODO: #mmalofeev Should it be some hardcoded value or some value from the settings?
    return QnResourceDisplayInfo(cameraResource).toString(detailLevel);
}

QString SoftTriggerEvent::detailing() const
{
    return tr("Trigger: %1").arg(m_triggerName);
}

FilterManifest SoftTriggerEvent::filterManifest()
{
    return {};
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
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
