// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

#include "../event_fields/input_port_field.h"
#include "../event_fields/source_camera_field.h"
#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& CameraInputEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<CameraInputEvent>(),
        .displayName = tr("Input Signal on Camera"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<SourceCameraField>("cameraId", tr("Camera")),
            makeFieldDescriptor<InputPortField>("inputPortId", tr("Input ID")),
        }
    };
    return kDescriptor;
}

CameraInputEvent::CameraInputEvent(
    std::chrono::microseconds timestamp,
    State state,
    QnUuid cameraId,
    const QString& inputPortId)
    :
    BasicEvent(timestamp, state),
    m_cameraId(cameraId),
    m_inputPortId(inputPortId)
{
}

QString CameraInputEvent::uniqueName() const
{
    return makeName(BasicEvent::uniqueName(), m_inputPortId);
}

QMap<QString, QString> CameraInputEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString CameraInputEvent::detailing() const
{
    return tr("Input Port: %1").arg(m_inputPortId);
}

} // namespace nx::vms::rules
