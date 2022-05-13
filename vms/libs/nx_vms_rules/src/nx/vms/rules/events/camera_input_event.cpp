// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

#include "../utils/event_details.h"

namespace nx::vms::rules {

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

FilterManifest CameraInputEvent::filterManifest()
{
    return {};
}

} // namespace nx::vms::rules
