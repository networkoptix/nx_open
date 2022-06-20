// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_context.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/network/cloud_system_endpoint.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <watchers/cloud_status_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

#include "cross_system_camera_resource.h"
#include "cross_system_server_resource.h"

namespace nx::vms::client::desktop {

using namespace nx::network::http;

namespace {

using namespace std::chrono;

static constexpr auto kUpdateInterval = 30s;
static const nx::vms::api::SoftwareVersion kRestApiSupportedVersion("5.0.0.0");

} // namespace

struct CloudCrossSystemContext::Private
{
    Private(CloudCrossSystemContext* owner, QnSystemDescriptionPtr systemDescription):
        q(owner),
        systemDescription(systemDescription)
    {
        NX_VERBOSE(this, "Initialized");

        auto timer = new QTimer(q);
        timer->setInterval(kUpdateInterval);
        timer->callOnTimeout([this]() { updateCameras(); });
        timer->start();

        auto updateConnectionStatus =
            [this](const QString& logMessage)
            {
                return [this, logMessage] { NX_VERBOSE(this, logMessage); ensureConnection(); };
            };

        connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, q,
            updateConnectionStatus("Cloud watcher status changed"));
        connect(systemDescription.get(), &QnBaseSystemDescription::reachableStateChanged, q,
            updateConnectionStatus("System reachable state changed"));
        connect(systemDescription.get(), &QnBaseSystemDescription::onlineStateChanged, q,
            updateConnectionStatus("System online state changed"));
        connect(systemDescription.get(), &QnBaseSystemDescription::system2faEnabledChanged, q,
            updateConnectionStatus("System 2fa support changed"));
        connect(systemDescription.get(), &QnBaseSystemDescription::oauthSupportedChanged, q,
            updateConnectionStatus("System OAuth support changed"));

        ensureConnection();
    }

    CloudCrossSystemContext* const q;
    QnSystemDescriptionPtr const systemDescription;
    core::RemoteConnectionFactory::ProcessPtr connectionProcess;
    std::unique_ptr<SystemContext> systemContext;
    bool isRestApiSupported = false;
    QnUserResourcePtr user;
    CrossSystemServerResourcePtr server;

    /**
     * Debug log representation. Used by toString(const T*).
     */
    QString idForToStringFromPtr() const { return systemDescription->name(); }

    bool ensureConnection()
    {
        if (systemContext)
            return true;

        if (qnCloudStatusWatcher->status() != QnCloudStatusWatcher::Online)
            return false;

        NX_VERBOSE(this, "Ensure connection exists");
        if (!connectionProcess)
        {
            auto handleConnection =
                [this](core::RemoteConnectionFactory::ConnectionOrError result)
                {
                    if (auto connection = std::get_if<core::RemoteConnectionPtr>(&result))
                    {
                        NX_VERBOSE(this, "Connection successfully established");
                        initializeContext(*connection);
                        ensureUser();
                    }
                    else if (const auto error = std::get_if<core::RemoteConnectionError>(&result))
                    {
                        NX_WARNING(this, "Error while establishing connection %1", error->code);
                    }

                    connectionProcess.reset();
                };

            auto endpoint = core::cloudSystemEndpoint(systemDescription);
            if (!endpoint)
                return false;

            NX_VERBOSE(this, "Initialize new connection");
            connectionProcess = qnClientCoreModule->networkModule()->connectionFactory()->connect(
                {
                    .address = endpoint->address,
                    .credentials = {},
                    .userType = nx::vms::api::UserType::cloud,
                    .expectedServerId = endpoint->serverId
                },
                handleConnection);
        }

        return false;
    }

    void initializeContext(core::RemoteConnectionPtr connection)
    {
        isRestApiSupported = (connection->moduleInformation().version >= kRestApiSupportedVersion);

        systemContext = std::make_unique<SystemContext>(appContext()->peerId());
        systemContext->setConnection(connection);

        server = CrossSystemServerResourcePtr(new CrossSystemServerResource(connection));

        systemContext->resourcePool()->addResource(server);
        server->setStatus(nx::vms::api::ResourceStatus::online);

        connect(systemContext->resourcePool(),
            &QnResourcePool::resourcesAdded,
            q,
            [this](const QnResourceList& resources)
            {
                auto cameras = resources.filtered<QnVirtualCameraResource>();
                if (!cameras.empty())
                    emit q->camerasAdded(cameras);
            });
        connect(systemContext->resourcePool(),
            &QnResourcePool::resourcesRemoved,
            q,
            [this](const QnResourceList& resources)
            {
                auto cameras = resources.filtered<QnVirtualCameraResource>();
                if (!cameras.empty())
                    emit q->camerasRemoved(cameras);
            });
        appContext()->addSystemContext(systemContext.get());
    }

    void addCamerasToResourcePool(std::vector<nx::vms::api::DeviceModel> cameras)
    {
        if (!NX_ASSERT(systemContext))
            return;

        auto resourcePool = systemContext->resourcePool();
        auto camerasToRemove = resourcePool->getAllCameras();
        QnResourceList newlyCreatedCameras;
        for (const auto& cameraModel: cameras)
        {
            if (auto existingCamera = resourcePool->getResourceById<CrossSystemCameraResource>(
                cameraModel.id))
            {
                camerasToRemove.removeOne(existingCamera);
                existingCamera->update(cameraModel);
            }
            else
            {
                auto camera = CrossSystemCameraResourcePtr(
                    new CrossSystemCameraResource(cameraModel));
                camera->setParentId(server->getId());
                newlyCreatedCameras.push_back(camera);
            }
        }

        if (!newlyCreatedCameras.empty())
            resourcePool->addResources(newlyCreatedCameras);
        if (!camerasToRemove.empty())
            resourcePool->removeResources(camerasToRemove);

        for (const auto& cameraModel: cameras)
        {
            auto camera = resourcePool->getResourceById<CrossSystemCameraResource>(
                cameraModel.id);

            if (!NX_ASSERT(camera))
                continue;

            if (cameraModel.status)
                camera->setStatus(*cameraModel.status);
            for (const auto& [name, value]: cameraModel.parameters)
                camera->setProperty(name, value.toString(), /*markDirty*/ false);
        }
    }

    void addUserToResourcePool(nx::vms::api::UserModel model)
    {
        NX_ASSERT(model.type == nx::vms::api::UserType::cloud);

        if (user)
            return;

        user = QnUserResourcePtr(new QnUserResource(model.type));
        user->setIdUnsafe(model.id);
        user->setName(model.name);
        user->setOwner(model.isOwner);
        user->setFullName(model.fullName);
        user->setEmail(model.email);

        // Avoid resources access recalculation. Inaccessible resources are filtered on a server
        // side.
        auto fixedPermissions = model.permissions
            | nx::vms::api::GlobalPermission::accessAllMedia;
        user->setRawPermissions(fixedPermissions);

        auto resourcePool = systemContext->resourcePool();
        resourcePool->addResource(user);
        systemContext->accessController()->setUser(user);
    }

    bool ensureUser()
    {
        if (user)
            return true;

        if (!NX_ASSERT(systemContext))
            return false;

        if (!isRestApiSupported)
            return false;

        if (!systemDescription->isReachable())
        {
            NX_VERBOSE(this, "System is unreachable");
            return false;
        }

        NX_VERBOSE(this, "Requesting user");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                if (!success)
                {
                     NX_WARNING(this, "User request failed");
                     return;
                }

                nx::vms::api::UserModel result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "User reply cannot be deserialized");
                    return;
                }
                addUserToResourcePool(std::move(result));
                updateCameras();
            });

        const QString encodedUsername =
            QUrl::toPercentEncoding(
                QString::fromStdString(systemContext->connection()->credentials().username));
        systemContext->connectedServerApi()->getRawResult(
            QString("/rest/v1/users/") + encodedUsername,
            {},
            callback,
            q->thread());

        return false;
    }

    void updateCameras()
    {
        if (!ensureConnection() || !ensureUser())
            return;

        if (!isRestApiSupported)
            return;

        if (!systemDescription->isReachable())
        {
            NX_VERBOSE(this, "System is unreachable");
            return;
        }

        NX_VERBOSE(this, "Updating cameras");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                if (!success)
                {
                     NX_WARNING(this, "Cameras request failed");
                     return;
                }

                std::vector<nx::vms::api::DeviceModel> result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "Cameras list cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "Received %1 cameras", result.size());
                addCamerasToResourcePool(std::move(result));
            });

        systemContext->connectedServerApi()->getRawResult(
            "/rest/v1/devices",
            {},
            callback,
            q->thread());
    }
};

CloudCrossSystemContext::CloudCrossSystemContext(
    QnSystemDescriptionPtr systemDescription,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, systemDescription))
{
}

CloudCrossSystemContext::~CloudCrossSystemContext()
{
}

SystemContext* CloudCrossSystemContext::systemContext() const
{
    return d->systemContext.get();
}

QnVirtualCameraResourceList CloudCrossSystemContext::cameras() const
{
    if (!d->systemContext)
        return {};

    return d->systemContext->resourcePool()->getAllCameras();
}

QString CloudCrossSystemContext::idForToStringFromPtr() const
{
    return d->idForToStringFromPtr();
}

} // namespace nx::vms::client::desktop
