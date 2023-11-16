// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_notifications_listener.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/ssl/helpers.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/p2p/transport/p2p_websocket_transport.h>
#include <nx/utils/buffer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/cloud_notification.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/utils/serialization.h>

namespace nx::vms::client::desktop {

namespace http = nx::network::http;
namespace websocket = nx::network::websocket;
static const std::chrono::seconds kKeepAliveTimeout(40);
static const std::chrono::seconds kReconnectTimeout(5);
static const QString kCloudNotificationType = "notification";
static const QString kApiCloudNotificationPath = "/cloud_notifications/provider/api/v1/subscribe";

using namespace nx::vms::api;

class CrossSystemNotificationsListener::Private
{
public:
    Private(CrossSystemNotificationsListener* owner): q(owner) {}
    ~Private()
    {
        nx::utils::promise<void> promise;
        reconnectTimer.pleaseStop(
            [this, &promise]()
            {
                p2pWebsocketTransport.reset();
                httpClient.reset();
                promise.set_value();
            });

        promise.get_future().wait();
    }

public:
    CrossSystemNotificationsListener* const q;
    nx::Buffer buffer;
    nx::network::aio::Timer reconnectTimer;
    std::unique_ptr<http::AsyncClient> httpClient;
    std::unique_ptr<nx::network::AbstractStreamSocket> socket;
    std::unique_ptr<nx::p2p::P2PWebsocketTransport> p2pWebsocketTransport;

    void reconnectWebsocket()
    {
        reconnectTimer.pleaseStop(
            [this]()
            {
                stopSync();

                core::CloudUrlHelper cloudUrlHelper(
                    nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
                    nx::vms::utils::SystemUri::ReferralContext::None);

                auto urlCloud = nx::utils::Url::fromQUrl(cloudUrlHelper.mainUrl());

                const auto address = nx::network::SocketAddress::fromUrl(urlCloud, true);

                http::HttpHeaders httpHeaders;
                websocket::addClientHeaders(&httpHeaders,
                    websocket::kWebsocketProtocolName,
                    websocket::CompressionType::none);

                auto const credentials = qnCloudStatusWatcher->credentials();

                httpClient =
                    std::make_unique<http::AsyncClient>(nx::network::ssl::kAcceptAnyCertificate);
                httpClient->bindToAioThread(reconnectTimer.getAioThread());
                httpClient->setAdditionalHeaders(httpHeaders);
                httpClient->setCredentials(credentials);

                QUrlQuery query;
                query.addQueryItem(
                    "access-token", QString::fromStdString(credentials.authToken.value));

                const auto url = nx::network::url::Builder()
                    .setScheme(urlCloud.scheme().toStdString())
                    .setEndpoint(address)
                    .setPath(kApiCloudNotificationPath)
                    .setQuery(query)
                    .toUrl();

                httpClient->doUpgrade(url,
                    http::Method::get,
                    websocket::kWebsocketProtocolName,
                    [this] { onUpgradeHttpClient(); });
            });
    }

private:

    void stopSync()
    {
        if (httpClient)
            httpClient->pleaseStopSync();

        if (p2pWebsocketTransport)
            p2pWebsocketTransport->pleaseStopSync();
    }

    void onUpgradeHttpClient()
    {
        if (httpClient->failed()
            || httpClient->response()->statusLine.statusCode
                != http::StatusCode::switchingProtocols)
        {
            NX_DEBUG(this, "Cloud connection error: %1", httpClient->lastSysErrorCode());
            scheduleReconnect();
            return;
        }

        socket = httpClient->takeSocket();
        httpClient.reset();

        if (!socket)
        {
            NX_DEBUG(this, "Http client has problems with the socket");
            return;
        }

        socket->setNonBlockingMode(true);

        p2pWebsocketTransport =
            std::make_unique<nx::p2p::P2PWebsocketTransport>(std::move(socket),
                websocket::Role::client,
                websocket::FrameType::text,
                websocket::CompressionType::none,
                kKeepAliveTimeout);

        p2pWebsocketTransport->start();

        NX_VERBOSE(this, "Websocket upgrade success");
        readSomeAsync();
    }

    void scheduleReconnect()
    {
        NX_DEBUG(this, "Next connection attempt in %1 seconds", kReconnectTimeout.count());
        reconnectTimer.start(kReconnectTimeout, [this]() { reconnectWebsocket(); });
        buffer.clear();
    }

    void onReadSomeAsync()
    {
        CloudNotification cloudMessage;
        auto deserializationSucceeded = QJson::deserialize(buffer, &cloudMessage);

        NX_VERBOSE(this, "Read message, deserialized: %1, type: %2",
            deserializationSucceeded, cloudMessage.type);

        auto systemId = appContext()->currentSystemContext()->globalSettings()->cloudSystemId();

        if (deserializationSucceeded && cloudMessage.type == kCloudNotificationType
            && (ini().allowOwnCloudNotifications ? true : cloudMessage.systemId != systemId))
        {
            QSharedPointer<nx::vms::rules::NotificationAction> notificationAction(
                new nx::vms::rules::NotificationAction);
            nx::vms::rules::deserializeProperties(
                cloudMessage.notification, notificationAction.get());
            emit q->notificationActionReceived(notificationAction, cloudMessage.systemId);
        }

        readSomeAsync();
    }

    void readSomeAsync()
    {
        p2pWebsocketTransport->readSomeAsync(&buffer,
            [this](SystemError::ErrorCode errorCode, std::size_t /*size*/)
            {
                if (errorCode == SystemError::noError)
                {
                    onReadSomeAsync(); //< Data is read from buffer.
                }
                else
                {
                    NX_DEBUG(this, "Failed to get data from the cloud. Error code: %1", errorCode);
                    scheduleReconnect();
                }
            });
    }
};

CrossSystemNotificationsListener::CrossSystemNotificationsListener(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    qRegisterMetaType<QSharedPointer<nx::vms::rules::NotificationAction>>();

    NX_ASSERT(qnCloudStatusWatcher, "Cloud status watcher is not ready");

    d->reconnectWebsocket();
}

CrossSystemNotificationsListener::~CrossSystemNotificationsListener()
{
}

} // namespace nx::vms::client::desktop
