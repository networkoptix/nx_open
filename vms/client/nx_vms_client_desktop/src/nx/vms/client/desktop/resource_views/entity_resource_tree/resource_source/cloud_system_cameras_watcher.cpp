// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_cameras_watcher.h"

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/network/cloud_system_endpoint.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;
using namespace nx::network;
using namespace nx::network::http;
using namespace nx::vms::api;

CloudSystemCamerasWatcher::CloudSystemCamerasWatcher(const QString& systemId, QObject* parent):
    base_type(parent),
    m_systemId(systemId)
{
    startTimer(updateInterval);
}

bool CloudSystemCamerasWatcher::ensureConnection()
{
    if (!m_connection && !m_connectionProcess)
    {
        auto handleConnection =
            [this](RemoteConnectionFactory::ConnectionOrError result)
            {
                if (auto connection = std::get_if<RemoteConnectionPtr>(&result))
                    m_connection = *connection;

                m_connectionProcess.reset();
            };

        auto endpoint = cloudSystemEndpoint(m_systemId);
        if (!endpoint)
            return false;

        m_connectionProcess = qnClientCoreModule->networkModule()->connectionFactory()->connect(
            {
                .address = endpoint->address,
                .credentials = {},
                .userType = nx::vms::api::UserType::cloud,
                .expectedServerId = endpoint->serverId
            },
            handleConnection);

        return false;
    }

    return m_connection.get();
}

void CloudSystemCamerasWatcher::updateCameras()
{
    m_connection->serverApi()->getCameras(
        nx::utils::guarded(this,
            [this](
                bool success,
                ::rest::Handle requestId,
                ::rest::ServerConnection::Cameras result)
            {
                if (!success)
                    return;

                QSet<QString> cameras;
                for (const nx::vms::api::DeviceModel& data: result)
                    cameras.insert(data.name);

                setCameras(cameras);
            }),
        this->thread());
}

void CloudSystemCamerasWatcher::timerEvent(QTimerEvent*)
{
    if (!ensureConnection())
        return;

    updateCameras();
}

void CloudSystemCamerasWatcher::setCameras(const QSet<Camera>& cameras)
{
    const auto addedCameras = cameras - m_cameras;
    const auto removedCameras = m_cameras - cameras;
    m_cameras = cameras;
    emit camerasAdded(addedCameras);
    emit camerasRemoved(removedCameras);
}

} // namespace nx::vms::client::desktop
