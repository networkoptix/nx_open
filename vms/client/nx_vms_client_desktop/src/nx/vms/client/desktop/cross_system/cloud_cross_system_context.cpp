// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_context.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <helpers/system_helpers.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/network/cloud_system_endpoint.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resources/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <watchers/cloud_status_watcher.h>

#include "cross_system_camera_resource.h"
#include "cross_system_layout_resource.h"
#include "cross_system_server_resource.h"
#include "cloud_layouts_manager.h"

namespace nx::vms::client::desktop {

using namespace nx::network::http;

namespace {

using namespace std::chrono;

static constexpr auto kUpdateInterval = 10min;
static const nx::vms::api::SoftwareVersion kRestApiSupportedVersion("5.0.0.0");

} // namespace

struct CloudCrossSystemContext::Private
{
    Private(CloudCrossSystemContext* owner, QnSystemDescriptionPtr systemDescription):
        q(owner),
        systemDescription(systemDescription)
    {
        NX_VERBOSE(this, "Initialized");

        systemContext = std::make_unique<SystemContext>(
            SystemContext::Mode::crossSystem,
            appContext()->peerId(),
            nx::core::access::Mode::direct);
        appContext()->addSystemContext(systemContext.get());

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

        if (ini().crossSystemLayouts)
        {
            connect(
                appContext()->cloudLayoutsSystemContext()->resourcePool(),
                &QnResourcePool::resourcesAdded,
                q,
                [this](const QnResourceList& resources)
                {
                    if (status != Status::connected)
                        createThumbCameraResources(resources.filtered<CrossSystemLayoutResource>());
                });
        }

        auto timer = new QTimer(q);
        timer->setInterval(kUpdateInterval);
        timer->callOnTimeout([this]() { updateCameras(); });
        timer->start();

        connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, q,
            [this]
            {
                NX_VERBOSE(this, "Cloud status became %1", qnCloudStatusWatcher->status());
                ensureConnection();
            });
        connect(systemDescription.get(), &QnBaseSystemDescription::reachableStateChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System became %1",
                    this->systemDescription->isReachable() ? "reachable" : "unreachable");
                ensureConnection();
            });
        connect(systemDescription.get(), &QnBaseSystemDescription::onlineStateChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System became %1",
                    this->systemDescription->isOnline() ? "online" : "offline");
                ensureConnection();
            });
        connect(systemDescription.get(), &QnBaseSystemDescription::system2faEnabledChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System 2fa support became %1",
                    this->systemDescription->is2FaEnabled() ? "enabled" : "disabled");
                ensureConnection();
            });
        connect(systemDescription.get(), &QnBaseSystemDescription::oauthSupportedChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System OAuth became %1",
                    this->systemDescription->isOauthSupported() ? "supported" : "unsupported");
                ensureConnection();
            });

        ensureConnection();
    }

    CloudCrossSystemContext* const q;
    QnSystemDescriptionPtr const systemDescription;
    Status status = Status::uninitialized;
    core::RemoteConnectionFactory::ProcessPtr connectionProcess;
    std::unique_ptr<SystemContext> systemContext;
    bool isRestApiSupported = false;
    QnUserResourcePtr user;
    CrossSystemServerResourcePtr server;

    /**
     * Debug log representation.
     */
    QString toString() const { return systemDescription->name(); }

    /**
     * Debug log representation. Used by toString(const T*).
     */
    QString idForToStringFromPtr() const { return toString(); }

    void updateStatus(Status value)
    {
        if (status == value)
            return;

        status = value;
        emit q->statusChanged();
    }

    bool ensureConnection(bool allowUserInteraction = false)
    {
        if (!ini().crossSystemLayouts)
            return false;

        if (status == Status::unsupported)
            return false;

        if (systemContext->connection())
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
                        updateStatus(Status::connectionFailure);
                    }

                    connectionProcess.reset();
                };

            auto endpoint = core::cloudSystemEndpoint(systemDescription);
            if (!endpoint)
                return false;

            updateStatus(Status::connecting);

            NX_VERBOSE(this, "Initialize new connection");
            connectionProcess = qnClientCoreModule->networkModule()->connectionFactory()->connect(
                {
                    .address = endpoint->address,
                    .credentials = {},
                    .userType = nx::vms::api::UserType::cloud,
                    .expectedServerId = endpoint->serverId,
                    .userInteractionAllowed = allowUserInteraction
                },
                handleConnection);
        }

        return false;
    }

    void initializeContext(core::RemoteConnectionPtr connection)
    {
        isRestApiSupported = (connection->moduleInformation().version >= kRestApiSupportedVersion);
        if (!isRestApiSupported)
        {
            updateStatus(Status::unsupported);
            return;
        }

        systemContext->setConnection(connection);

        server = CrossSystemServerResourcePtr(new CrossSystemServerResource(connection));
        server->setIdUnsafe(connection->moduleInformation().id);

        systemContext->resourcePool()->addResource(server);
        server->setStatus(nx::vms::api::ResourceStatus::online);
    }

    void addServersToResourcePool(std::vector<nx::vms::api::MediaServerData> servers)
    {
        if (!NX_ASSERT(systemContext))
            return;

        const QString cloudSystemId = systemContext->moduleInformation().cloudSystemId;

        QnResourceList newServers;
        for (const auto& server: servers)
        {
            if (server.id == this->server->getId())
            {
                this->server->setName(server.name);
                continue;
            }

            const QString cloudAddress = core::helpers::serverCloudHost(cloudSystemId, server.id);

            CrossSystemServerResourcePtr newServer(new CrossSystemServerResource(
                server.id,
                cloudAddress.toStdString(),
                systemContext->connection()));
            newServer->setName(server.name);

            newServers.push_back(newServer);
        }

        const auto resourcePool = systemContext->resourcePool();
        if (!newServers.empty())
            resourcePool->addResources(newServers);

        // We do not update servers periodically, so count them all as online.
        for (const auto& serverResource: newServers)
            serverResource->setStatus(nx::vms::api::ResourceStatus::online);
    }

    void addCamerasToResourcePool(std::vector<nx::vms::api::CameraDataEx> cameras)
    {
        if (!NX_ASSERT(systemContext))
            return;

        auto resourcePool = systemContext->resourcePool();
        auto camerasToRemove = resourcePool->getAllCameras();
        QnResourceList newlyCreatedCameras;
        for (const auto& cameraData: cameras)
        {
            if (auto existingCamera = resourcePool->getResourceById<CrossSystemCameraResource>(
                cameraData.id))
            {
                camerasToRemove.removeOne(existingCamera);
                existingCamera->update(cameraData);
            }
            else
            {
                auto camera = CrossSystemCameraResourcePtr(
                    new CrossSystemCameraResource(q, cameraData));
                newlyCreatedCameras.push_back(camera);
            }
        }

        if (!newlyCreatedCameras.empty())
            resourcePool->addResources(newlyCreatedCameras);
        if (!camerasToRemove.empty())
            resourcePool->removeResources(camerasToRemove);

        for (const auto& cameraData: cameras)
        {
            auto camera = resourcePool->getResourceById<CrossSystemCameraResource>(
                cameraData.id);

            if (!NX_ASSERT(camera))
                continue;

            camera->setStatus(cameraData.status);
            for (const auto& param: cameraData.addParams)
                camera->setProperty(param.name, param.value, /*markDirty*/ false);
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

        if (!NX_ASSERT(isRestApiSupported))
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
                updateServers();
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

    void updateServers()
    {
        if (!ensureConnection() || !ensureUser())
            return;

        if (!systemDescription->isReachable())
        {
            NX_VERBOSE(this, "System is unreachable");
            return;
        }

        NX_VERBOSE(this, "Updating servers");
        auto callback = nx::utils::guarded(q,
            [this](
                bool success,
                ::rest::Handle requestId,
                QByteArray data,
                nx::network::http::HttpHeaders /*headers*/)
            {
                if (!success)
                {
                     NX_WARNING(this, "Servers request failed");
                     return;
                }

                std::vector<nx::vms::api::MediaServerData> result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "Cameras list cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "Received %1 servers", result.size());
                addServersToResourcePool(std::move(result));
                updateCameras();
            });

        systemContext->connectedServerApi()->getRawResult(
            "/ec2/getMediaServers",
            {},
            callback,
            q->thread());
    }

    void updateCameras()
    {
        if (!ensureConnection() || !ensureUser())
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

                std::vector<nx::vms::api::CameraDataEx> result;
                if (!QJson::deserialize(data, &result))
                {
                    NX_WARNING(this, "Cameras list cannot be deserialized");
                    return;
                }

                NX_VERBOSE(this, "Received %1 cameras", result.size());
                addCamerasToResourcePool(std::move(result));
                updateStatus(Status::connected);
            });

        systemContext->connectedServerApi()->getRawResult(
            "/ec2/getCamerasEx",
            {},
            callback,
            q->thread());
    }

    void createThumbCameraResources(const CrossSystemLayoutResourceList& crossSystemLayoutsList)
    {
        // Required to find all the cloud resource descriptors inside the cross system layouts
        // that still not have an actual Resource. It might be happen by the various
        // reasons(system not found at the time or system found, but user action required or
        // system is loading). Than create a thumb resource in the resource pool until the real
        // resource is appeared. When the real resource is appeared, the thumb resources must be
        // updated.
        for (const auto& layout: crossSystemLayoutsList)
        {
            for (const auto& item: layout->getItems())
            {
                if (crossSystemResourceSystemId(item.resource) != systemDescription->id())
                    continue;

                const auto camera = CrossSystemCameraResourcePtr(
                    new CrossSystemCameraResource(q, item.resource));

                systemContext->resourcePool()->addResource(camera);
            }
        }
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

CloudCrossSystemContext::~CloudCrossSystemContext() = default;

CloudCrossSystemContext::Status CloudCrossSystemContext::status() const
{
    return d->status;
}

bool CloudCrossSystemContext::isConnected() const
{
    return d->status == Status::connected;
}

QString CloudCrossSystemContext::systemId() const
{
    return d->systemDescription->id();
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
    return d->toString();
}

QString CloudCrossSystemContext::toString() const
{
    return d->toString();
}

void CloudCrossSystemContext::initializeConnectionWithUserInteraction()
{
    d->connectionProcess.reset();
    d->updateStatus(Status::connecting);
    d->ensureConnection(/*allowUserInteraction*/ true);
}

void CloudCrossSystemContext::update(UpdateReason reason)
{
    switch (reason)
    {
        case UpdateReason::new_:
        {
            if (ini().crossSystemLayouts)
            {
                d->createThumbCameraResources(appContext()->cloudLayoutsSystemContext()
                    ->resourcePool()->getResources<CrossSystemLayoutResource>());
            }
            break;
        }
        case UpdateReason::found:
        {
            if (d->ensureConnection())
                d->updateCameras();

            break;
        }
        case UpdateReason::lost:
        {
            if (d->status != Status::unsupported)
                d->updateStatus(Status::uninitialized);

            break;
        }
    }
}

QString toString(CloudCrossSystemContext::Status status)
{
    switch (status)
    {
        case CloudCrossSystemContext::Status::uninitialized:
            return CloudCrossSystemContext::tr("Inaccessible");
        case CloudCrossSystemContext::Status::connecting:
            return CloudCrossSystemContext::tr("Loading...");
        case CloudCrossSystemContext::Status::connectionFailure:
            return CloudCrossSystemContext::tr("Information required");
        case CloudCrossSystemContext::Status::unsupported:
            return "UNSUPPORTED"; //< Debug purposes.
        case CloudCrossSystemContext::Status::connected:
            return "CONNECTED"; //< Debug purposes.
        default:
            return {};
    }
}

} // namespace nx::vms::client::desktop
