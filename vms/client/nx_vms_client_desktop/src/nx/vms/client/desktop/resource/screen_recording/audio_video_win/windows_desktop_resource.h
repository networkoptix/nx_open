// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_camera_connection.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>

#include "../desktop_data_provider_wrapper.h"

class QOpenGLWidget;

namespace nx::vms::client::desktop {

class DesktopDataProvider;

class WindowsDesktopResource: public core::DesktopResource
{
public:
    WindowsDesktopResource(QOpenGLWidget* mainWindow = nullptr);
    virtual ~WindowsDesktopResource();

    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

private:
    QnAbstractStreamDataProvider* createDataProviderInternal();

    void createSharedDataProvider();

    friend class DesktopDataProviderWrapper;

private:
    QOpenGLWidget* const m_mainWidget;
    DesktopDataProvider* m_desktopDataProvider = nullptr;
    nx::Mutex m_dpMutex;
};

} // namespace nx::vms::client::desktop
