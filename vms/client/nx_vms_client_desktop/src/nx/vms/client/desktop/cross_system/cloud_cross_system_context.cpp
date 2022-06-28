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
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/network/cloud_system_endpoint.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
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
                        updateStatus(Status::connectionFailure);
                    }

                    connectionProcess.reset();
                };

            updateStatus(Status::connecting);

            auto endpoint = core::cloudSystemEndpoint(systemDescription);
            if (!endpoint)
                return false;

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

        systemContext = std::make_unique<SystemContext>(
            appContext()->peerId(),
            nx::core::access::Mode::direct);
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
                    new CrossSystemCameraResource(cameraData));
                camera->setParentId(server->getId());
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

CloudCrossSystemContext::Status CloudCrossSystemContext::status() const
{
    return d->status;
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

} // namespace nx::vms::client::desktop
