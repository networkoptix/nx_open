// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_desktop_camera_initializer.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/videowall/desktop_camera_connection_controller.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_navigator.h>

using TwoWayAudioAvailabilityWatcher = nx::vms::client::core::TwoWayAudioAvailabilityWatcher;

namespace nx::vms::client::desktop {

struct TwoWayAudioDesktopCameraInitializer::Private
{
    TwoWayAudioDesktopCameraInitializer* q = nullptr;

    TwoWayAudioAvailabilityWatcher* watcher = nullptr;

    Private(TwoWayAudioDesktopCameraInitializer* parent):
        q(parent),
        watcher(new TwoWayAudioAvailabilityWatcher(q))
    {
    }

    void initializeDesktopCameraConnection()
    {
        if (!watcher->available())
            return;

        auto desktopCameraConnectionController =
            SystemContext::fromResource(watcher->camera())->desktopCameraConnectionController();
        if (NX_ASSERT(desktopCameraConnectionController))
            desktopCameraConnectionController->initialize();
    }
};

TwoWayAudioDesktopCameraInitializer::TwoWayAudioDesktopCameraInitializer(
    WindowContext* context,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(context),
    d(new Private(this))
{
    connect(d->watcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        [this]
        {
            d->initializeDesktopCameraConnection();
        });

    connect(windowContext()->navigator(), &QnWorkbenchNavigator::currentResourceChanged, this,
        [this]()
        {
            const auto selectedCamera = windowContext()->navigator()->currentResource()
                .dynamicCast<QnVirtualCameraResource>();

            d->watcher->setCamera(selectedCamera);
            d->initializeDesktopCameraConnection();
        });
}

TwoWayAudioDesktopCameraInitializer::~TwoWayAudioDesktopCameraInitializer()
{
}

} // namespace nx::vms::client::desktop
