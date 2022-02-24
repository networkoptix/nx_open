// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_cameras_source.h"

#include "cloud_system_cameras_watcher.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CloudSystemCamerasSource::CloudSystemCamerasSource(const QString& systemId)
{
    initializeRequest =
        [this, systemId]
        {
            m_cloudSystemCamerasWatcher = std::make_unique<CloudSystemCamerasWatcher>(systemId);

            m_connectionsGuard.add(
                QObject::connect(
                    m_cloudSystemCamerasWatcher.get(),
                    &CloudSystemCamerasWatcher::camerasAdded,
                    [this](const CloudSystemCamerasWatcher::Cameras& cameras)
                    {
                        for (auto& camera: cameras)
                            (*addKeyHandler)(camera);
                    }));

            m_connectionsGuard.add(
                QObject::connect(
                    m_cloudSystemCamerasWatcher.get(),
                    &CloudSystemCamerasWatcher::camerasRemoved,
                    [this](const CloudSystemCamerasWatcher::Cameras& cameras)
                    {
                        for (auto& camera: cameras)
                            (*removeKeyHandler)(camera);
                    }));
        };
}

CloudSystemCamerasSource::~CloudSystemCamerasSource()
{
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
