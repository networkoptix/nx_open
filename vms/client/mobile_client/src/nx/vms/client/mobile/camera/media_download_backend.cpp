// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_download_backend.h"

#include <QtGui/QDesktopServices>
#include <QtQml/QtQml>

#include <api/http_client_pool.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::mobile {

using ClientPool = nx::network::http::ClientPool;

struct VideoDownloadContext
{
    std::chrono::milliseconds startTime;
    std::chrono::milliseconds duration;
    nx::Uuid cameraId;
    QString cloudSystemId;
    nx::Uuid serverId;
};

using VideoDownloadCallback = std::function<void (VideoDownloadContext context)>;

namespace {

nx::network::url::Builder relayUrlBuilder(const QString& systemId, const QString& path)
{
    return nx::network::url::Builder(
        nx::format("https://%1.relay.vmsproxy.com", systemId).toQString()).setPath(path);
}

} // namespace

bool sameCameras(const QnVirtualCameraResourcePtr& left, const QnVirtualCameraResourcePtr& right)
{
    const auto getId =
        [](const auto& device)
        {
            return device
                ? device->getId()
                : nx::Uuid{};
        };

    return getId(left) == getId(right);
}

struct MediaDownloadBackend::Private
{
    MediaDownloadBackend* const q;
    bool downloadAvailable = false;
    nx::Uuid deviceId;
    QnVirtualCameraResourcePtr camera;

    std::unique_ptr<ClientPool> clientPool = std::make_unique<ClientPool>();

    QString currentCloudSystemId() const;
    void updateDownloadAvailability();
    void gatherProxyServerId(VideoDownloadContext&& context, VideoDownloadCallback&& callback);
    void runDownloadForContext(const VideoDownloadContext& context, const QString& ticket);
};

QString MediaDownloadBackend::Private::currentCloudSystemId() const
{
    const auto server = q->currentServer();
    return server
        ? server->getModuleInformation().cloudSystemId
        : QString{};
}

void MediaDownloadBackend::Private::updateDownloadAvailability()
{
    const auto manager = q->sessionManager();
    const auto currentUser = q->userWatcher()->user();
    const bool hasExportArchivePermission = camera
        && q->resourceAccessManager()->hasAccessRights(
            currentUser, camera, api::AccessRight::exportArchive);
    const bool hasWatermark = q->systemSettings()->watermarkSettings().useWatermark
        && !q->resourceAccessManager()->hasPowerUserPermissions(currentUser);
    const bool value = manager
        && manager->hasConnectedSession()
        && manager->isCloudSession()
        && manager->connectedServerVersion() >= utils::SoftwareVersion(6, 0)
        && hasExportArchivePermission
        && !hasWatermark
        && qnSettings->useDownloadVideoFeature();

    if (value == downloadAvailable)
        return;

    downloadAvailable = value;
    emit q->downloadAvailabilityChanged();
}

void MediaDownloadBackend::Private::gatherProxyServerId(VideoDownloadContext&& context,
    VideoDownloadCallback&& callback)
{
    static const nx::Uuid kAdapterFuncId = nx::Uuid::createUuid();
    NX_VERBOSE(this, "Cloud system <%1>: send moduleInformation request", context.cloudSystemId);

    ClientPool::Request request;
    request.method = nx::network::http::Method::get;
    request.url = relayUrlBuilder(context.cloudSystemId, "/api/moduleInformation");

    ClientPool::ContextPtr requestContext(new ClientPool::Context(
        kAdapterFuncId,
        nx::network::ssl::kAcceptAnyCertificate));
    requestContext->request = request;
    requestContext->completionFunc = nx::utils::AsyncHandlerExecutor(q).bind(
        [this, context = std::move(context), callback = std::move(callback)]
        (ClientPool::ContextPtr requestContext) mutable
        {
            nx::network::rest::JsonResult jsonReply;
            nx::vms::api::ModuleInformationWithAddresses moduleInformation;
            if (!requestContext->hasSuccessfulResponse()
                || !QJson::deserialize(requestContext->response.messageBody, &jsonReply)
                || !QJson::deserialize(jsonReply.reply, &moduleInformation))
            {
                q->showDownloadProcessError();
                return;
            }

            context.serverId = moduleInformation.id;
            callback(std::move(context));
        });

    requestContext->setTargetThread(q->thread());
    clientPool->sendRequest(requestContext);
}

void MediaDownloadBackend::Private::runDownloadForContext(const VideoDownloadContext& context,
    const QString& ticket)
{
    const auto path = QString("/rest/v3/devices/%1/media.mp4")
        .arg(context.cameraId.toSimpleString());
    const auto url = relayUrlBuilder(context.cloudSystemId, path)
        .addQueryItem("positionMs", QString::number(context.startTime.count()))
        .addQueryItem("durationMs", QString::number(context.duration.count()))
        .addQueryItem("download", "true")
        .addQueryItem("signature", "true")
        .addQueryItem("rotation", "auto")
        .addQueryItem("continuousTimestamps", "true")
        .addQueryItem("_ticket", ticket)
        .addQueryItem("X-server-guid", context.serverId.toSimpleString())
        .toUrl().toQUrl();

    NX_DEBUG(this, "Made request to download media: %1", url.toString());

    QDesktopServices::openUrl(url);
}

//-------------------------------------------------------------------------------------------------

void MediaDownloadBackend::registerQmlType()
{
    qmlRegisterType<MediaDownloadBackend>("nx.vms.client.mobile", 1, 0, "MediaDownloadBackend");
}

MediaDownloadBackend::MediaDownloadBackend(QObject* parent):
    QObject(parent),
    base_type(SystemContext::fromQmlContext(this)),
    d(new Private{.q = this})
{
    const auto initializeDelayed =
        [this]()
        {
            const auto manager = sessionManager();
            const auto update = [this]() { d->updateDownloadAvailability(); };
            connect(manager, &SessionManager::sessionHostChanged, this, update);
            connect(manager, &SessionManager::hasConnectedSessionChanged, this, update);
            connect(manager, &SessionManager::connectedServerVersionChanged, this, update);
            connect(systemSettings(), &common::SystemSettings::watermarkChanged, this, update);
            connect(userWatcher(), &core::UserWatcher::userChanged, this, update);
            connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
                [update](int propertyId)
                {
                    if (propertyId == QnMobileClientSettings::UseDownloadVideoFeature)
                        update();
                });
            update();
        };

    executeLater(initializeDelayed, this);
}

MediaDownloadBackend::~MediaDownloadBackend()
{
}

bool MediaDownloadBackend::isDownloadAvailable() const
{
    return d->downloadAvailable;
}

void MediaDownloadBackend::downloadVideo(qint64 startTimeMs,
    qint64 durationMs)
{
    // Download video process consist of the few next steps:
    // 1. Request relay for module information of currently available server to proxy the request
    // 2. Create a one-time authorization ticket for exacly this server.
    // 3. Generate an url for media download with specified proxy server, ticket and other params.
    // 4. Open this url in the external default browser and let it download the media file.

    if (startTimeMs == -1)
        startTimeMs = qnSyncTime->currentMSecsSinceEpoch();

    VideoDownloadContext context = {
        .startTime = std::chrono::milliseconds(startTimeMs),
        .duration = std::chrono::milliseconds(durationMs),
        .cameraId = d->camera->getId(),
        .cloudSystemId = d->currentCloudSystemId()
    };

    if (!NX_ASSERT(!context.cloudSystemId.isEmpty() && isDownloadAvailable()))
    {
        showDownloadProcessError();
        return;
    }

    const auto requestTicketHandler =
        [this](VideoDownloadContext&& context)
        {
            const auto api = connectedServerApi();
            if (!NX_ASSERT(api))
            {
                showDownloadProcessError();
                return;
            }

            using ResponseData = rest::ErrorOrData<nx::vms::api::LoginSession>;
            auto tryOpenDownload = nx::utils::guarded(this,
                [this, context = std::move(context)]
                (bool /*success*/, int /*handle*/, ResponseData errorOrData)
                {
                    if (auto session = std::get_if<nx::vms::api::LoginSession>(&errorOrData))
                    {
                        d->runDownloadForContext(std::move(context), session->token.c_str());
                        return;
                    }

                    if (const auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                        NX_DEBUG(this, "Can't get authorization ticket, error %1", error->errorId);
                    else
                        NX_ASSERT(false, "Shouldn't get here");

                    showDownloadProcessError();
                });

            api->createTicket(context.serverId, std::move(tryOpenDownload), thread());
        };

    d->gatherProxyServerId(std::move(context), requestTicketHandler);
}

nx::Uuid MediaDownloadBackend::cameraId() const
{
    return d->deviceId;
}

void MediaDownloadBackend::setCameraId(const nx::Uuid& value)
{
    if (d->deviceId == value)
        return;

    d->deviceId = value;
    emit cameraIdChanged();

    const auto newCamera = resourcePool()->getResourceById<QnVirtualCameraResource>(value);
    if (sameCameras(d->camera, newCamera))
        return;

    d->camera = newCamera;
    d->updateDownloadAvailability();
}

void MediaDownloadBackend::showDownloadProcessError()
{
    emit errorOccured(tr("Can't download video"),
        tr("Please check a network connection."));
}

} // namespace nx::vms::client::mobile
