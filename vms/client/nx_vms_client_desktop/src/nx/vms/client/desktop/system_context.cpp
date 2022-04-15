// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/analytics/object_display_settings.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {



struct SystemContext::Private
{
    std::unique_ptr<QnWorkbenchAccessController> accessController;
    std::unique_ptr<ObjectDisplaySettings> objectDisplaySettings;
    std::unique_ptr<VideoWallOnlineScreensWatcher> videoWallOnlineScreensWatcher;
};

SystemContext::SystemContext(QnUuid peerId, QObject* parent):
    nx::vms::client::core::SystemContext(
        std::move(peerId),
        parent),
    d(new Private())
{
    d->accessController = std::make_unique<QnWorkbenchAccessController>(this);
    d->objectDisplaySettings = std::make_unique<ObjectDisplaySettings>();
    d->videoWallOnlineScreensWatcher = std::make_unique<VideoWallOnlineScreensWatcher>(
        this);
}

SystemContext::~SystemContext()
{
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    return dynamic_cast<SystemContext*>(resource->systemContext());
}

QnWorkbenchAccessController* SystemContext::accessController() const
{
    return d->accessController.get();
}

ObjectDisplaySettings* SystemContext::objectDisplaySettings() const
{
    return d->objectDisplaySettings.get();
}

VideoWallOnlineScreensWatcher* SystemContext::videoWallOnlineScreensWatcher() const
{
    return d->videoWallOnlineScreensWatcher.get();
}

} // namespace nx::vms::client::desktop
