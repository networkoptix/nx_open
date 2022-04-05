// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_event.h"

#include "../event_fields/source_camera_field.h"
#include "../event_fields/source_user_field.h"
#include "../event_fields/text_field.h"

namespace nx::vms::rules {

QString SoftTriggerEvent::caption() const
{
    return m_caption.isEmpty()
        ? QString("%1 %2").arg(manifest().displayName).arg(m_triggerName)
        : m_caption;
}

void SoftTriggerEvent::setCaption(const QString& caption)
{
    m_caption = caption;
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
        .description = "",
        .fields = {
            makeFieldDescriptor<SourceCameraField>("cameraId", tr("Camera ID")),
            makeFieldDescriptor<SourceUserField>("userId", tr("User ID")),
            makeFieldDescriptor<EventTextField>("triggerName", tr("Name"))
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
