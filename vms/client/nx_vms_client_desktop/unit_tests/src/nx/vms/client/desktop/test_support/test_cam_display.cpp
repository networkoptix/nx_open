// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_cam_display.h"

namespace nx::vms::client::desktop::test {

TestCamDisplay::TestCamDisplay(const QString& name, CameraID cameraID):
    name(name), cameraID(cameraID)
{
}

void TestCamDisplay::setQuality(MediaQuality quality, bool fastSwitch)
{
    qualityValue = quality;
    fastSwitchValue = fastSwitch;
}

void TestCamDisplay::setCallbackForStreamChanges(std::function<void()> callback)
{
    m_streamCallback = callback;
}

void TestCamDisplay::executeStreamChangedCallback()
{
    m_streamCallback();
}
} // namespace nx::vms::client::desktop::test
