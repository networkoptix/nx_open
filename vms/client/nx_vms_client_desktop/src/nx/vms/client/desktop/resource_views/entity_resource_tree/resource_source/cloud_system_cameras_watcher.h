// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>

namespace nx::vms::client::desktop {

class CloudSystemCamerasWatcher:
    public QObject,
    public core::RemoteConnectionAware
{
    Q_OBJECT

public:
    using base_type = QObject;

    using Camera = QString;
    using Cameras = QSet<QString>;

    static constexpr auto updateInterval = std::chrono::seconds(1);

public:
    CloudSystemCamerasWatcher(const QString& systemId, QObject* parent = nullptr);

signals:
    void camerasAdded(const Cameras& cameras);
    void camerasRemoved(const Cameras& cameras);

protected:
    virtual void timerEvent(QTimerEvent*) override;

private:
    void updateCameras();
    void setCameras(const QSet<Camera>& cameras);
    bool ensureConnection();

private:
    QString m_systemId;
    Cameras m_cameras;
    core::RemoteConnectionPtr m_connection;
    core::RemoteConnectionFactory::ProcessPtr m_connectionProcess;
};

} // namespace nx::vms::client::desktop
