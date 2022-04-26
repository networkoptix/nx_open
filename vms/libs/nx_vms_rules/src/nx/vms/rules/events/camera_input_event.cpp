// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

namespace nx::vms::rules {

QString CameraInputEvent::uniqueName() const
{
    return makeName(BasicEvent::uniqueName(), m_inputPortId);
}

FilterManifest CameraInputEvent::filterManifest()
{
    return {};
}

} // namespace nx::vms::rules
