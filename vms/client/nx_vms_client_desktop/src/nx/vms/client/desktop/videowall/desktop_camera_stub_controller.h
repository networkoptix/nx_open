// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>

#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Creates and removes desktop camera stub resource when there are layouts which contain it.
 * DesktopCameraPreloaderResource initially created as a mock for real desktop camera
 * before server creates the real instance afterwards.
 * When the server creates desktop camera, it updates correspondent DesktopCameraPreloaderResource,
 * and it loses its Qn::fake flag.
 */
class DesktopCameraStubController:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DesktopCameraStubController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~DesktopCameraStubController();

    QnVirtualCameraResourcePtr createLocalCameraStub(const QString& physicalId);

private:
    void atResourcesAdded(const QnResourceList& resources);
    void atResourcesRemoved(const QnResourceList& resources);
    void atUserChanged();
    void decreaseUsageCount(const nx::Uuid& resourceId);
    void disconnectLayoutConnection(const nx::Uuid& layoutId);

private:
    DesktopCameraPreloaderResourcePtr m_localCameraStub;

    std::multiset<nx::Uuid> m_desktopCameraUsage;
    QMap<nx::Uuid, QMetaObject::Connection> m_desktopCameraLayoutConnections;
};

} // namespace nx::vms::client::desktop
