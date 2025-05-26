// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_context.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <core/resource/camera_history.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <network/system_helpers.h>
#include <nx/analytics/properties.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_access_controller.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "cloud_cross_system_context_data_loader.h"
#include "cloud_layouts_manager.h"
#include "cross_system_camera_resource.h"
#include "cross_system_layout_resource.h"
#include "cross_system_server_resource.h"

namespace nx::vms::client::core {

using namespace nx::network::http;
using namespace nx::cloud::db::api;

namespace {

using namespace std::chrono;

static constexpr auto kUpdateConnectionInterval = 30s;
static const nx::utils::SoftwareVersion kMinApiSupportedVersion(5, 1);

struct ContextRemover
{
    void operator()(SystemContext* context)
    {
        appContext()->removeSystemContext(context);
    }
};

} // namespace

struct CloudCrossSystemContext::Private
{
    Private(CloudCrossSystemContext* owner, SystemDescriptionPtr systemDescription):
        q(owner),
        systemDescription(systemDescription)
    {
        NX_VERBOSE(this, "Initialized");

        systemContext.reset(appContext()->createSystemContext(SystemContext::Mode::crossSystem));

        crossSystemAccessController = static_cast<CrossSystemAccessController*>(
            systemContext->accessController());

        systemContext->startModuleDiscovery(
            new nx::vms::discovery::Manager(systemContext.get()));
        systemContext->moduleDiscoveryManager()->startModuleConnectorOnly();

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

        if (!systemDescription->version().isNull()
            && systemDescription->version() < kMinApiSupportedVersion)
        {
            // TODO: #mmalofeev add an ability to handle system version changes.
            updateStatus(Status::unsupportedPermanently);
            return;
        }

        if (appContext()->cloudCrossSystemManager()->connectingAutomatically())
        {
            ensureConnection();
            setupAutomaticReconnection();
        }

        tokenUpdater = std::make_unique<core::CloudSessionTokenUpdater>(q);
        connect(
            tokenUpdater.get(),
            &core::CloudSessionTokenUpdater::sessionTokenExpiring,
            q,
            [this]{ issueAccessToken(); });
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
    }

    CloudCrossSystemContext* const q;
    SystemDescriptionPtr const systemDescription;
    Status status = Status::uninitialized;
    core::RemoteConnectionFactory::ProcessPtr connectionProcess;
    // Context should be destroyed after dataLoader.
    std::unique_ptr<SystemContext, ContextRemover> systemContext;
    std::unique_ptr<CloudCrossSystemContextDataLoader> dataLoader;
    CrossSystemAccessController* crossSystemAccessController = nullptr;
    core::UserResourcePtr user;
    core::CrossSystemServerResourcePtr server;
    std::unique_ptr<core::CloudSessionTokenUpdater> tokenUpdater;
    bool automaticReconnection = false;
    bool needsCloudAuthorization = false;
    int retryCount = 0;

    void setupAutomaticReconnection()
    {
        if (status == Status::unsupportedPermanently)
            return;

        auto timer = new QTimer(q);
        timer->setInterval(kUpdateConnectionInterval);
        timer->callOnTimeout(
            [this]()
            {
                updateTokenUpdater();
                ensureConnection();
            });
        timer->start();

        automaticReconnection = true;

        connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::statusChanged, q,
            [this]
            {
                NX_VERBOSE(this, "Cloud status became %1", qnCloudStatusWatcher->status());
                ensureConnection();
            });

        connect(systemDescription.get(), &SystemDescription::reachableStateChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System became %1",
                    this->systemDescription->isReachable() ? "reachable" : "unreachable");
                ensureConnection();
            });

        connect(systemDescription.get(), &SystemDescription::onlineStateChanged, q,
            [this]
            {
                const bool isOnline = this->systemDescription->isOnline();
                NX_VERBOSE(this, "System became %1", isOnline ? "online" : "offline");
                ensureConnection();
            });

        connect(systemDescription.get(), &SystemDescription::system2faEnabledChanged, q,
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

        connect(systemDescription.get(), &SystemDescription::oauthSupportedChanged, q,
            [this]
            {
                NX_VERBOSE(
                    this,
                    "System OAuth became %1",
                    this->systemDescription->isOauthSupported() ? "supported" : "unsupported");
                ensureConnection();
            });
    }

    void updateTokenUpdater()
    {
        auto connection = systemContext->connection();
        if (!connection)
            return;

        auto callback = nx::utils::guarded(
            q,
            [this](
                bool /*success*/,
                int /*handle*/,
                rest::ErrorOrData<nx::vms::api::LoginSession> session)
            {
                if (session)
                {
                    tokenUpdater->onTokenUpdated(
                        qnSyncTime->currentTimePoint() + session->expiresInS);

                    // If tokenUpdater is not active, it means that the recently issued access token
                    // had a short lifespan (less than 20 seconds), and tokenUpdater won't have
                    // enough time to refresh it. As a result, the internal timer will not be
                    // started. This can happen, for example, when Cloud Session Limiter is enabled,
                    // and the refresh token was not reissued. In this case, it can be assumed that
                    // cross-system context requires the refresh token to be reissued.
                    needsCloudAuthorization = !tokenUpdater->isActive();
                }
                else
                {
                    if (session.error().errorId == nx::network::rest::ErrorId::sessionTruncated)
                        needsCloudAuthorization = true;

                    NX_VERBOSE(this, "Update token error: %1", session.error().errorId);
                }
            });

        if (const auto api = connection->serverApi(); NX_ASSERT(api, "No Server connection"))
            api->getCurrentSession(std::move(callback), QThread::currentThread());
    }

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

        if (!automaticReconnection && status != Status::connecting)
            setupAutomaticReconnection();

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
            [this, allowUserInteraction](core::RemoteConnectionFactory::ConnectionOrError result)
            {
                if (auto connection = std::get_if<core::RemoteConnectionPtr>(&result))
                {
                    NX_VERBOSE(this, "Connection successfully established");
                    initializeContext(*connection);
                }
                else if (const auto error = std::get_if<core::RemoteConnectionError>(&result))
                {
                    if (error->code == core::RemoteConnectionErrorCode::truncatedSessionToken
                        && allowUserInteraction && retryCount == 0)
                    {
                        connectionProcess.reset();
                        emit q->cloudAuthorizationRequested(QPrivateSignal{});
                        ensureConnection(true);
                        retryCount++;
                        return;
                    }

                    retryCount = 0;

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

        connectionProcess = appContext()->networkModule()->connectionFactory()->connect(
            *logonData, handleConnection, systemContext.get());

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

        const nx::Uuid serverId = connection->moduleInformation().id;
        server = core::CrossSystemServerResourcePtr(
            new core::CrossSystemServerResource(systemContext.get()));
        server->setIdUnsafe(serverId);
        server->setName(connection->moduleInformation().name);

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
            QString::fromStdString(connection->credentials().username),
            connection->moduleInformation().version);

        QObject::connect(dataLoader.get(),
            &CloudCrossSystemContextDataLoader::ready,
            q,
            [this]()
            {
                addUserToResourcePool(dataLoader->user());
                if (const auto userGroups = dataLoader->userGroups())
                    addUserGroups(userGroups.value());
                addServersToResourcePool(dataLoader->servers());
                systemContext->cameraHistoryPool()->resetServerFootageData(
                    dataLoader->serverFootageData());
                systemContext->globalSettings()->update(dataLoader->systemSettings());
                systemContext->licensePool()->replaceLicenses(dataLoader->licenses());
                if (auto connection = systemContext->connection())
                    updateTokenUpdater();
                addCamerasToResourcePool(dataLoader->cameras());
                addServerTaxonomyDescriptions(dataLoader->serverTaxonomyDescriptions());
                updateStatus(Status::connected);
            });

        QObject::connect(dataLoader.get(),
            &CloudCrossSystemContextDataLoader::camerasUpdated,
            q,
            [this]()
            {
                addCamerasToResourcePool(dataLoader->cameras());
            });

        QObject::connect(qnCloudStatusWatcher,
            &nx::vms::client::core::CloudStatusWatcher::credentialsChanged,
            q,
            [this]()
            {
                if (needsCloudAuthorization)
                    issueAccessToken();
            });

        dataLoader->start(/*requestUser*/ !user);
    }

    void issueAccessToken()
    {
        auto cloudSystemId = systemContext->globalSettings()->cloudSystemId();
        if (cloudSystemId.isEmpty() && systemDescription)
            cloudSystemId = systemDescription->id();

        const auto remoteConnectionCredentials =
            qnCloudStatusWatcher->remoteConnectionCredentials();

        IssueTokenRequest request{
            .grant_type = GrantType::refresh_token,
            .client_id = core::CloudConnectionFactory::clientId(),
            .scope = nx::format("cloudSystemId=%1", cloudSystemId).toStdString(),
            .refresh_token = remoteConnectionCredentials.authToken.value
        };

        auto callback = nx::utils::guarded(
            q,
            [this](ResultCode result, IssueTokenResponse response)
            {
                if (result != ResultCode::ok)
                {
                    NX_VERBOSE(this, "Issue access token error result: %1", result);
                    updateStatus(Status::connectionFailure);
                    return;
                }

                if (response.error)
                {
                    NX_VERBOSE(this, "Issue access token error response: %1", response.error);
                    updateStatus(Status::connectionFailure);
                    return;
                }

                auto connection = systemContext->connection();
                if (!connection)
                {
                    updateStatus(Status::connectionFailure);
                    return;
                }

                auto credentials = connection->credentials();
                credentials.authToken = nx::network::http::BearerAuthToken(response.access_token);
                connection->updateCredentials(credentials, response.expires_at);
                updateTokenUpdater();
                dataLoader->requestData();
            });

        tokenUpdater->issueToken(request, std::move(callback), q);
    }

    void addServersToResourcePool(nx::vms::api::ServerInformationV1List servers)
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

            core::CrossSystemServerResourcePtr newServer(new core::CrossSystemServerResource(
                server.id, cloudSystemId.toStdString(), systemContext.get()));

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

    void addServerTaxonomyDescriptions(const nx::vms::api::ServerModelV1List& serversData)
    {
        if (!NX_ASSERT(systemContext))
            return;

        for (const auto& serverData: serversData)
        {
            const auto server = systemContext->resourcePool()->getResourceById(serverData.id);

            const auto descriptorsIter =
                serverData.parameters.find(nx::analytics::kDescriptorsProperty);
            if (descriptorsIter == serverData.parameters.end())
                continue;

            QJsonDocument doc(descriptorsIter->second.toObject());
            server->setProperty(
                nx::analytics::kDescriptorsProperty,
                doc.toJson(QJsonDocument::Compact));
        }
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
            if (auto existingCamera = resourcePool->getResourceById<core::CrossSystemCameraResource>(
                cameraData.id))
            {
                camerasToRemove.removeOne(existingCamera);
                existingCamera->update(cameraData);
            }
            else
            {
                auto camera = core::CrossSystemCameraResourcePtr(
                    new core::CrossSystemCameraResource(systemDescription->id(), cameraData));
                newlyCreatedCameras.push_back(camera);
            }
        }

        if (!newlyCreatedCameras.empty())
            resourcePool->addResources(newlyCreatedCameras);
        if (!camerasToRemove.empty())
            resourcePool->removeResources(camerasToRemove);

        for (const auto& cameraData: cameras)
        {
            auto camera = resourcePool->getResourceById<core::CrossSystemCameraResource>(
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

    void addUserGroups(const nx::vms::api::UserGroupDataList& groups)
    {
        auto userGroupManager = systemContext->userGroupManager();
        userGroupManager->resetAll(groups);

        for (const auto& group: groups)
        {
            systemContext->accessRightsManager()->setOwnResourceAccessMap(group.id,
                {group.resourceAccessRights.begin(), group.resourceAccessRights.end()});
        }
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
                if (core::crossSystemResourceSystemId(item.resource) != systemDescription->id())
                    continue;

                createThumbCameraResource(item.resource.id, item.resource.name);
            }
        }
    }

    QnVirtualCameraResourcePtr createThumbCameraResource(const nx::Uuid& id, const QString& name)
    {
        const auto camera = core::CrossSystemCameraResourcePtr(
            new core::CrossSystemCameraResource(systemDescription->id(), id, name));
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
            systemContext->resourcePool()->getResources<core::CrossSystemCameraResource>())
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
    SystemDescriptionPtr systemDescription,
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

    const auto update =
        [this]()
        {
            d->updateCameras();
            d->crossSystemAccessController->updateAllPermissions();
        };

    connect(this, &CloudCrossSystemContext::statusChanged, this, update);
    connect(systemDescription.data(), &SystemDescription::onlineStateChanged, this, update);
    connect(systemDescription.data(), &SystemDescription::isCloudSystemChanged, this, update);
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

SystemDescriptionPtr CloudCrossSystemContext::systemDescription() const
{
    return d->systemDescription;
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
    const nx::Uuid& id,
    const QString& name)
{
    return d->createThumbCameraResource(id, name);
}

bool CloudCrossSystemContext::needsCloudAuthorization()
{
    return d->needsCloudAuthorization;
}

void CloudCrossSystemContext::cloudAuthorize()
{
    if (!d->systemContext->connection())
    {
        initializeConnectionWithUserInteraction();
        return;
    }

    // Unsuccessful cloud re-authorization can cause deletion of this context.
    const QPointer guard(this);

    emit cloudAuthorizationRequested(QPrivateSignal{});

    if (guard)
        d->issueAccessToken();
}

nx::Uuid  CloudCrossSystemContext::organizationId() const
{
    const auto organizationId = d->systemContext->globalSettings()->organizationId();
    return !organizationId.isNull() ? organizationId : d->systemDescription->organizationId();
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

} // namespace nx::vms::client::core
