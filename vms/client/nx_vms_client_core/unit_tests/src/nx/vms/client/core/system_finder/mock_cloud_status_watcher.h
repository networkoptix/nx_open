// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/abstract_cloud_status_watcher.h>

namespace nx::vms::client::core::test {

class MockCloudStatusWatcher: public AbstractCloudStatusWatcher
{
    Q_OBJECT

public:
    explicit MockCloudStatusWatcher(QObject* parent = nullptr);

    Status status() const override;
    QnCloudSystemList cloudSystems() const override;

    void setMockStatus(Status status);
    void setMockCloudSystems(const QnCloudSystemList& systems);

private:
    Status m_status = LoggedOut;
    QnCloudSystemList m_systems;
};

} // namespace nx::vms::client::core::test
