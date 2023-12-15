// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_context.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <camera/camera_data_manager.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_history.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <licensing/license.h>
#include <network/system_helpers.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>

#include "cloud_cross_system_context_data_loader.h"
#include "cloud_layouts_manager.h"
#include "cross_system_camera_resource.h"
#include "cross_system_layout_resource.h"
#include "cross_system_server_resource.h"

namespace nx::vms::client::desktop {

using namespace nx::network::http;

namespace {

using namespace std::chrono;

static constexpr auto kUpdateConnectionInterval = 30s;
static const nx::utils::SoftwareVersion kMinApiSupportedVersion(5, 1);

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

       systemContext->resourceDataPool()->setExternalSource(
           appContext()->currentSystemContext()->resourceDataPool());

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

        connect(
            appContext()->cloudLayoutsSystemContext()->resourcePool(),
            &QnResourcePool::resourcesAdded,
            q,
            [this](const QnResourceList& resources)
            {
                if (status != Status::connected)
                    createThumbCameraResources(resources.filtered<CrossSystemLayoutResource>());
            });

        auto timer = new QTimer(q);
        timer->setInterval(kUpdateConnectionInterval);
        timer->callOnTimeout([this]() { ensureConnection(); });
        timer->start();

        connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::statusChanged, q,
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
                const bool isOnline = this->systemDescription->isOnline();
                NX_VERBOSE(this, "System became %1", isOnline ? "online" : "offline");
                ensureConnection();
            });
        connect(systemDescription.get(), &QnBaseSystemDescription::system2faEnabledChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System 2fa support became %1",
                    this->systemDescription->is2FaEnabled() ? "enabled" : "disabled");

                if (status == Status::connected && !is2FaCompatible())
                    updateStatus(Status::unsupportedTemporary);
                else
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

        if (!systemDescription->version().isNull()
            && systemDescription->version() < kMinApiSupportedVersion)
        {
            // TODO: #mmalofeev add an ability to handle system version changes.
            updateStatus(Status::unsupportedPermanently);
            return;
        }

        ensureConnection();
    }

    ~Private()
    {
        // Emit signals about all resources removing. This is a temporary measure: more accurate
        // solution would be to listen `systemContextRemoved` in all appropriate places, but it
        // is way more complex.
        const auto resources = systemContext->resourcePool()->getResources();
        const auto resourcePool = systemContext->resourcePool();
        resourcePool->disconnect(q);
        resourcePool->removeResources(resources);

        appContext()->removeSystemContext(systemContext.get());
    }

    CloudCrossSystemContext* const q;
    std::unique_ptr<CloudCrossSystemContextDataLoader> dataLoader;
    QnSystemDescriptionPtr const systemDescription;
    Status status = Status::uninitialized;
    core::RemoteConnectionFactory::ProcessPtr connectionProcess;
    std::unique_ptr<SystemContext> systemContext;
    core::UserResourcePtr user;
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

        const auto oldStatus = status;
        status = value;

        emit q->statusChanged(oldStatus);
    }

    /** Returns true if new connection is started. */
    bool ensureConnection(bool allowUserInteraction = false)
    {
        if (!systemDescription->isOnline())
            return false;

        if (status == Status::unsupportedPermanently)
            return false;

        if (status == Status::unsupportedTemporary && !is2FaCompatible())
            return false;

        if (systemContext->connection())
            return false;

        NX_VERBOSE(this, "Ensure connection exists");
        if (qnCloudStatusWatcher->status() != core::CloudStatusWatcher::Online)
        {
            NX_VERBOSE(this, "Cloud status failure: %1", qnCloudStatusWatcher->status());
            return false;
        }

        if (connectionProcess)
        {
            NX_VERBOSE(this, "Connection is already in progress");
            return false;
        }

        auto logonData = core::cloudLogonData(systemDescription);
        if (!logonData)
        {
            NX_VERBOSE(this, "Endpoint was not found");
            return false;
        }
        logonData->purpose = core::LogonData::Purpose::connectInCrossSystemMode;
        logonData->userInteractionAllowed = allowUserInteraction;

        auto handleConnection =
            [this](core::RemoteConnectionFactory::ConnectionOrError result)
            {
                if (auto connection = std::get_if<core::RemoteConnectionPtr>(&result))
                {
                    NX_VERBOSE(this, "Connection successfully established");
                    initializeContext(*connection);
                }
                else if (const auto error = std::get_if<core::RemoteConnectionError>(&result))
                {
                    NX_WARNING(this, "Error while establishing connection %1", error->code);

                    auto status = Status::connectionFailure;

                    if (error->code == core::RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa
                        || error->code == core::RemoteConnectionErrorCode::versionIsTooLow)
                    {
                        status = Status::unsupportedPermanently;
                    }
                    else if (error->code
                        == core::RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled)
                    {
                        status = Status::unsupportedTemporary;
                    }

                    updateStatus(status);
                }

                connectionProcess.reset();
            };

        // To prevent icons and overlays blinking update status to the connecting state only when
        // the system is not initialized or when a user initiated reconnection process(system
        // status items and cross system cameras changes current icons to the loading icon)
        if (status == Status::uninitialized || allowUserInteraction)
            updateStatus(Status::connecting);

        NX_VERBOSE(this, "Initialize new connection");

        connectionProcess = qnClientCoreModule->networkModule()->connectionFactory()->connect(
            *logonData,
            handleConnection);

        return true;
    }

    void initializeContext(core::RemoteConnectionPtr connection)
    {
        systemContext->setConnection(connection);
        if (connection->moduleInformation().version < kMinApiSupportedVersion)
        {
            updateStatus(Status::unsupportedPermanently);
            return;
        }

        const QnUuid serverId = connection->moduleInformation().id;
        server = CrossSystemServerResourcePtr(new CrossSystemServerResource(connection));
        server->setIdUnsafe(serverId);

        // If resource with such id was added to the cross-system layout, camera thumb will be
        // created before server is added to the resource pool.
        if (auto thumb = systemContext->resourcePool()->getResourceById(serverId))
            systemContext->resourcePool()->removeResource(thumb);

        systemContext->resourcePool()->addResource(server);
        server->setStatus(nx::vms::api::ResourceStatus::online);

        if (const auto userModel = connection->connectionInfo().compatibilityUserModel)
            addUserToResourcePool(*userModel);

        dataLoader = std::make_unique<CloudCrossSystemContextDataLoader>(
            connection->serverApi(),
            QString::fromStdString(connection->credentials().username));
        QObject::connect(dataLoader.get(),
            &CloudCrossSystemContextDataLoader::ready,
            q,
            [this]()
            {
                addUserToResourcePool(dataLoader->user());
                addServersToResourcePool(dataLoader->servers());
                if (NX_ASSERT(systemContext))
                {
                    systemContext->cameraHistoryPool()->resetServerFootageData(
                        dataLoader->serverFootageData());
                    systemContext->globalSettings()->update(dataLoader->systemSettings());
                    systemContext->licensePool()->replaceLicenses(dataLoader->licenses());
                }
                addCamerasToResourcePool(dataLoader->cameras());
                updateStatus(Status::connected);
            });
        QObject::connect(dataLoader.get(),
            &CloudCrossSystemContextDataLoader::camerasUpdated,
            q,
            [this]()
            {
                addCamerasToResourcePool(dataLoader->cameras());
            });
        dataLoader->start(/*requestUser*/ !user);
    }

    void addServersToResourcePool(nx::vms::api::ServerInformationList servers)
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

            const QString cloudAddress = helpers::serverCloudHost(cloudSystemId, server.id);

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
                    new CrossSystemCameraResource(systemDescription->id(), cameraData));
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

    template<class UserModelData>
    void addUserToResourcePool(UserModelData model)
    {
        // Compatibility mode user can already be added to resource pool.
        if (user)
            return;

        NX_ASSERT(model.type == nx::vms::api::UserType::cloud);
        user = core::UserResourcePtr(new core::UserResource(model));

        auto resourcePool = systemContext->resourcePool();
        resourcePool->addResource(user);
        systemContext->accessController()->setUser(user);
    }

    void createThumbCameraResources(const CrossSystemLayoutResourceList& crossSystemLayoutsList)
    {
        // Required to find all the cloud resource descriptors inside the cross system layouts
        // that still not have an actual Resource. It might be happen by the various
        // reasons (system not found at the time or system found, but user action required or
        // system is loading). Than create a thumb resource in the resource pool until the real
        // resource is appeared. When the real resource is appeared, the thumb resources must be
        // updated.
        for (const auto& layout: crossSystemLayoutsList)
        {
            for (const auto& item: layout->getItems())
            {
                if (crossSystemResourceSystemId(item.resource) != systemDescription->id())
                    continue;

                createThumbCameraResource(item.resource.id, item.resource.name);
            }
        }
    }

    QnVirtualCameraResourcePtr createThumbCameraResource(const QnUuid& id, const QString& name)
    {
        const auto camera = CrossSystemCameraResourcePtr(
            new CrossSystemCameraResource(systemDescription->id(), id, name));
        systemContext->resourcePool()->addResource(camera);

        return camera;
    }

    void updateCameras()
    {
        const auto systemIsReadyToUse = q->isSystemReadyToUse();
        api::ResourceStatus dummyCameraStatus =
            (status == Status::connectionFailure && systemDescription->isOnline())
                ? api::ResourceStatus::unauthorized
                : api::ResourceStatus::offline;

        for (const auto& camera:
            systemContext->resourcePool()->getResources<CrossSystemCameraResource>())
        {
            if (systemIsReadyToUse)
            {
                camera->removeFlags(Qn::fake);

                if (NX_ASSERT(camera->source()))
                    camera->setStatus(camera->source()->status);
                else
                    camera->setStatus(api::ResourceStatus::online);

                const auto loader = systemContext->cameraDataManager()->loader(
                    camera, /*createIfNotExists*/ false);

                if (loader)
                    loader->load(/*forced*/ true);
            }
            else
            {
                camera->addFlags(Qn::fake);

                const bool forceNotifyStatusChanged = dummyCameraStatus == camera->getStatus();
                camera->setStatus(dummyCameraStatus);
                if (forceNotifyStatusChanged) //< For the context status dependent entities.
                    camera->statusChanged(camera, Qn::StatusChangeReason::Local);

                const auto loader = systemContext->cameraDataManager()->loader(
                    camera, /*createIfNotExists*/ false);

                if (loader)
                    loader->discardCachedData();
            }
        }
    }

    bool is2FaCompatible() const
    {
        return !systemDescription->is2FaEnabled() || qnCloudStatusWatcher->is2FaEnabledForUser();
    }
};

CloudCrossSystemContext::CloudCrossSystemContext(
    QnSystemDescriptionPtr systemDescription,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, systemDescription))
{
    executeDelayedParented(
        [this]
        {
            // The method below is called with the delay due to thumb camera resources accept
            // the context in the constructor, so it is required the context to be created first.
            d->createThumbCameraResources(appContext()->cloudLayoutsSystemContext()
                ->resourcePool()->getResources<CrossSystemLayoutResource>());
        },
        this
    );

    const auto update = [this]() { d->updateCameras(); };

    connect(this, &CloudCrossSystemContext::statusChanged, this, update);
    connect(systemDescription.data(), &QnBaseSystemDescription::onlineStateChanged, this, update);
    connect(systemDescription.data(), &QnBaseSystemDescription::isCloudSystemChanged, this, update);
}

CloudCrossSystemContext::~CloudCrossSystemContext() = default;

CloudCrossSystemContext::Status CloudCrossSystemContext::status() const
{
    return d->status;
}

bool CloudCrossSystemContext::isSystemReadyToUse() const
{
    // For example let consider when a cloud system SysA is reachable and online. So we start
    // the client, the context do it stuff and becomes connected. Then we switch off the SysA,
    // so it becomes offline. In that case the context is still connected but the system is
    // offline. When the system will be online again we could continue use connection from
    // the context.
    return d->status == Status::connected
        && d->systemDescription->isOnline()
        && d->systemDescription->isCloudSystem();
}

QString CloudCrossSystemContext::systemId() const
{
    return d->systemDescription->id();
}

QnBaseSystemDescription* CloudCrossSystemContext::systemDescription() const
{
    return d->systemDescription.get();
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

bool CloudCrossSystemContext::initializeConnectionWithUserInteraction()
{
    if (d->connectionProcess && d->connectionProcess->context->logonData.userInteractionAllowed)
    {
        NX_DEBUG(this, "Connection with user interaction is already in progress");
        return false;
    }

    d->connectionProcess.reset();
    return d->ensureConnection(/*allowUserInteraction*/ true);
}

QnVirtualCameraResourcePtr CloudCrossSystemContext::createThumbCameraResource(
    const QnUuid& id,
    const QString& name)
{
    return d->createThumbCameraResource(id, name);
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
            return CloudCrossSystemContext::tr("Click to Show Cameras");
        case CloudCrossSystemContext::Status::unsupportedPermanently:
            return "UNSUPPORTED PERMANENTLY"; //< Debug purposes.
        case CloudCrossSystemContext::Status::unsupportedTemporary:
            return "UNSUPPORTED TEMPORARY"; //< Debug purposes.
        case CloudCrossSystemContext::Status::connected:
            return "CONNECTED"; //< Debug purposes.
        default:
            return {};
    }
}

} // namespace nx::vms::client::desktop
