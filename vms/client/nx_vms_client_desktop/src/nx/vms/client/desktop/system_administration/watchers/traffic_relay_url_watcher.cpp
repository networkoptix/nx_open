// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "traffic_relay_url_watcher.h"

#include <QtCore/QJsonObject>
#include <QtCore/QTimer>
#include <QtCore/QXmlStreamReader>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

static const std::chrono::seconds kRequestRetryDelay(5);

static const QString kElementSet = "set";
static const QString kAttributeResName = "resName";
static const QString kAttributeResValue = "resValue";
static const QString kResNameValueRelay = "relay";
static const QString kCloudModulesPath = "/discovery/v2/cloud_modules.xml";

class TrafficRelayUrlWatcher::Private
{
public:
    Private(TrafficRelayUrlWatcher* owner): q(owner)
    {
        requestTrafficRelayUrl();
    }

    ~Private()
    {
        stopHttpClient();
    }

public:
    TrafficRelayUrlWatcher* const q;
    QTimer reconnectTimer;
    std::unique_ptr<nx::network::http::AsyncClient> httpClient;
    nx::utils::PendingOperation delayedRequestTrafficRelayUrl{
        [this]() { requestTrafficRelayUrl(); }, kRequestRetryDelay};
    QString trafficRelayUrl;
    mutable nx::Mutex mutex;

    QString getTrafficRelayUrl() const
    {
        NX_MUTEX_LOCKER lock(&mutex);
        return trafficRelayUrl;
    }

    void stopHttpClient()
    {
        if (!httpClient)
            return;

        httpClient->pleaseStopSync();
        httpClient.reset();
    }

    void requestTrafficRelayUrl()
    {
        stopHttpClient();

        const auto cloudSystemId = q->systemSettings()->cloudSystemId();
        if (cloudSystemId.isEmpty())
            return;

        const core::CloudUrlHelper cloudUrlHelper(
            nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
            nx::vms::utils::SystemUri::ReferralContext::None);

        const auto urlCloud = nx::utils::Url::fromQUrl(cloudUrlHelper.mainUrl());

        const auto address = nx::network::SocketAddress::fromUrl(urlCloud, true);

        httpClient = std::make_unique<nx::network::http::AsyncClient>(
            nx::network::ssl::kAcceptAnyCertificate);

        const auto url = nx::network::url::Builder()
            .setScheme(urlCloud.scheme().toStdString())
            .setEndpoint(address)
            .setPath(kCloudModulesPath)
            .toUrl();

        auto handler = nx::utils::AsyncHandlerExecutor(q).bind(
            [this, url]()
            {
                NX_MUTEX_LOCKER lock(&mutex);
                trafficRelayUrl = {};
                if (httpClient->failed())
                {
                    NX_VERBOSE(this, "GET request failed. Url: %1", url);
                    delayedRequestTrafficRelayUrl.requestOperation();
                    return;
                }

                const auto result = httpClient->fetchMessageBodyBuffer();
                stopHttpClient();
                if (result.empty())
                {
                    NX_VERBOSE(this, "GET body fetch failed. Url: %1", url);
                    delayedRequestTrafficRelayUrl.requestOperation();
                    return;
                }
                const auto xmlResult = result.toByteArray();
                QXmlStreamReader xmlReader(xmlResult);

                if (xmlReader.atEnd())
                {
                    NX_VERBOSE(this, "Can not deserialize GET response. Url: %1", url);
                    delayedRequestTrafficRelayUrl.requestOperation();
                    return;
                }

                while(!xmlReader.atEnd())
                {
                    xmlReader.readNext();

                    if (!xmlReader.isStartElement())
                        continue;

                    if (xmlReader.name().toString() == kElementSet)
                    {
                        const auto attributes = xmlReader.attributes();
                        if (attributes.hasAttribute(kAttributeResName)
                            && attributes.value(kAttributeResName).toString() == kResNameValueRelay
                            && attributes.hasAttribute(kAttributeResValue))
                        {
                            trafficRelayUrl = attributes.value(kAttributeResValue).toString();
                            break;
                        }
                    }
                }

                if (!trafficRelayUrl.isEmpty())
                    emit q->trafficRelayUrlReady();
            });

        httpClient->doGet(url, std::move(handler));
    }
};

TrafficRelayUrlWatcher::TrafficRelayUrlWatcher(SystemContext* context, QObject* parent):
    QObject(parent), SystemContextAware(context), d(new Private(this))
{
    connect(globalSettings(), &common::SystemSettings::cloudSettingsChanged,
        this, [this]() { d->delayedRequestTrafficRelayUrl.requestOperation(); });
}

QString TrafficRelayUrlWatcher::trafficRelayUrl() const
{
    return d->getTrafficRelayUrl();
}

TrafficRelayUrlWatcher::~TrafficRelayUrlWatcher()
{
}

} // namespace nx::vms::client::desktop
