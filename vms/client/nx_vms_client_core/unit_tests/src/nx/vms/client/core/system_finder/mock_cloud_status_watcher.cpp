// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mock_cloud_status_watcher.h"

namespace nx::vms::client::core::test {

MockCloudStatusWatcher::MockCloudStatusWatcher(QObject* parent):
    AbstractCloudStatusWatcher(parent)
{
}

AbstractCloudStatusWatcher::Status MockCloudStatusWatcher::status() const
{
    return m_status;
}

QnCloudSystemList MockCloudStatusWatcher::cloudSystems() const
{
    return m_systems;
}

void MockCloudStatusWatcher::setMockStatus(Status status)
{
    m_status = status;
    emit statusChanged(status);
}

void MockCloudStatusWatcher::setMockCloudSystems(const QnCloudSystemList& systems)
{
    m_systems = systems;
    emit cloudSystemsChanged(systems);
}

} // namespace nx::vms::client::core::test
