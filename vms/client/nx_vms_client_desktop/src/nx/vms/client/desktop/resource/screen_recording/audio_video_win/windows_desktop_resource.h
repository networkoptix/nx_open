// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_camera_connection.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>

namespace nx::vms::client::desktop {

class WindowsDesktopResource: public core::DesktopResource
{
public:
    WindowsDesktopResource();

    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    virtual QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role) override;

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;
};

} // namespace nx::vms::client::desktop
