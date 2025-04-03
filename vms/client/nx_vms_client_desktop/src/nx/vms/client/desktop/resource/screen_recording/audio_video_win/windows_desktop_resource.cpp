// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "windows_desktop_resource.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtMultimedia/QMediaDevices>

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_camera_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>

#include "desktop_data_provider.h"

namespace nx::vms::client::desktop {

WindowsDesktopResource::WindowsDesktopResource()
{
    addFlags(Qn::local_live_cam | Qn::desktop_camera);

    const QString name = "Desktop";
    setName(name);
    setUrl(name);

    // Only one desktop resource is allowed.
    setIdUnsafe(core::DesktopResource::getDesktopResourceUuid());
}

bool WindowsDesktopResource::hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const
{
    return true;
}

QnAbstractStreamDataProvider* WindowsDesktopResource::createDataProvider(
    Qn::ConnectionRole /*role*/)
{
    // According to legacy design, the caller will retain ownership of the created object, this
    // should be fixed in the future.
    return appContext()->createAudioInputProvider().release();
}

bool WindowsDesktopResource::isRendererSlow() const
{
    using namespace screen_recording;
    CaptureMode captureMode = screenRecordingSettings()->captureMode();
    return captureMode == CaptureMode::fullScreen;
}

static AudioLayoutConstPtr emptyAudioLayout(new AudioLayout());

AudioLayoutConstPtr WindowsDesktopResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    auto provider = dynamic_cast<const core::DesktopDataProviderBase*>(dataProvider);
    if (provider && provider->getAudioLayout())
        return provider->getAudioLayout();
    return emptyAudioLayout;
}

} // namespace nx::vms::client::desktop
