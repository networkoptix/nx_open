// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "traffic_relay_url_watcher.h"

#include <QtCore/QJsonObject>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

static const std::chrono::seconds kRequestRetryDelay(5);
static const QString kTrafficRelayUrlRequest = R"json(
    {
        "destinationHostName": "%1",
        "connectionMethods": 5,
        "cloudConnectVersion": "connectOverHttpHasHostnameAsString"
    })json";

static const QString kCloudPathTrafficRelayInfo = "/mediator/server/%1/sessions/";
static const QString kTrafficRelayUrl = "trafficRelayUrl";

class TraffiRelayUrlWatcher::Private
{
public:
    Private(TraffiRelayUrlWatcher* owner): q(owner) {}
    ~Private()
    {
        nx::utils::promise<void> promise;
        reconnectTimer.pleaseStop(
            [this, &promise]()
            {
                httpClient.reset();
                promise.set_value();
            });

        promise.get_future().wait();
    }

public:
    TraffiRelayUrlWatcher* const q;
    nx::network::aio::Timer reconnectTimer;
    std::unique_ptr<nx::network::http::AsyncClient> httpClient;
    QString trafficRelayUrl;
    mutable nx::Mutex mutex;

    QString getTrafficRelayUrl() const
    {
        NX_MUTEX_LOCKER lock(&mutex);
        return trafficRelayUrl;
    }

    void updateTrafficRelayUrl()
    {
        reconnectTimer.pleaseStop(
            [this]()
            {
                if (httpClient)
                    httpClient->pleaseStopSync();

                const auto cloudSystemId = q->systemSettings()->cloudSystemId();
                if (cloudSystemId.isEmpty())
                    return;

                const core::CloudUrlHelper cloudUrlHelper(
                    nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
                    nx::vms::utils::SystemUri::ReferralContext::None);

                const auto urlCloud = nx::utils::Url::fromQUrl(cloudUrlHelper.mainUrl());

                const nx::network::SocketAddress address{urlCloud.host().toStdString(),
                    (quint16) urlCloud.port(
                        nx::network::http::defaultPortForScheme(urlCloud.scheme().toStdString()))};

                httpClient = std::make_unique<nx::network::http::AsyncClient>(
                    nx::network::ssl::kAcceptAnyCertificate);
                httpClient->bindToAioThread(reconnectTimer.getAioThread());

                const auto url = nx::network::url::Builder()
                    .setScheme(urlCloud.scheme().toStdString())
                    .setEndpoint(address)
                    .setPath(nx::format(kCloudPathTrafficRelayInfo, cloudSystemId))
                    .toUrl();

                auto messageBody = std::make_unique<nx::network::http::BufferSource>(
                    Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
                    nx::format(kTrafficRelayUrlRequest, cloudSystemId).toStdString());

                httpClient->doPost(url, std::move(messageBody),
                    [this, url]()
                    {
                        NX_MUTEX_LOCKER lock(&mutex);
                        trafficRelayUrl = {};
                        if (httpClient->failed())
                        {
                            scheduleRequestRelayUrl();
                            NX_VERBOSE(this, "POST request failed. Url: %1", url);
                            return;
                        }

                        const auto result = httpClient->fetchMessageBodyBuffer();
                        httpClient.reset();
                        if (result.empty())
                        {
                            NX_VERBOSE(this, "POST body fetch failed. Url: %1", url);
                            return;
                        }

                        QJsonObject response;
                        if (nx::reflect::json::deserialize(result, &response))
                        {
                            trafficRelayUrl = response[kTrafficRelayUrl].toString();
                        }
                        else
                        {
                            NX_VERBOSE(this, "Can not deserialize POST response. Url: %1", url);
                        }

                        emit q->trafficRelayUrlReady();
                    });
            });
    }

private:

    void stopSync()
    {
        if (httpClient)
            httpClient->pleaseStopSync();
    }

    void scheduleRequestRelayUrl()
    {
        reconnectTimer.start(kRequestRetryDelay, [this]() { updateTrafficRelayUrl(); });
    }
};

TraffiRelayUrlWatcher::TraffiRelayUrlWatcher(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context),
    d(new Private(this))
{
    connect(globalSettings(),
        &common::SystemSettings::cloudSettingsChanged,
        this,
        [this]() { d->updateTrafficRelayUrl(); });

    d->updateTrafficRelayUrl();
}

QString TraffiRelayUrlWatcher::trafficRelayUrl() const
{
    return d->getTrafficRelayUrl();
}

TraffiRelayUrlWatcher::~TraffiRelayUrlWatcher()
{
}

} // namespace nx::vms::client::desktop
