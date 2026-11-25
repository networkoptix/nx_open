// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_service_checker.h"

#include <chrono>
#include <string>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_factory.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/json/flags.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/network/abstract_cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

namespace {

constexpr std::chrono::seconds kRequestRetryDelay(120);
constexpr int kMaxRetryAttempts = 3;
const QString kApiCloudSettingsPath = "/api/utils/settings";
const QString kFeatureFlagsKey = "featureFlags";

/** Cloud services by their flag names; placeholders, not yet agreed with the cloud team. */
NX_REFLECTION_ENUM_CLASS(CloudService, docdb, push_notifications, cloud_notifications);

/** The features that cannot work without the service. */
CloudFeatures featuresOf(CloudService service)
{
    switch (service)
    {
        case CloudService::docdb:
            return CloudFeature::vmsClientCloudLayouts;
        case CloudService::push_notifications:
            return CloudFeature::vmsClientPushNotifications;
        case CloudService::cloud_notifications:
            return CloudFeature::vmsClientCrossSiteNotifications;
    }

    return {};
}

/** The features of cloud services, which cloud instances before 26.2 do not report. */
constexpr CloudFeatures kFeaturesEnabledByDefault = CloudFeature::vmsClientCloudLayouts
    | CloudFeature::vmsClientPushNotifications | CloudFeature::vmsClientCrossSiteNotifications;

using nx::reflect::enumeration::fromStringStrict;

} // namespace

struct CloudServiceChecker::Private
{
    CloudServiceChecker* const q;
    AbstractCloudStatusWatcher* const cloudStatusWatcher;
    Settings* const settings;
    const nx::Url cloudPortalUrl; //< Empty means the portal of the current cloud host.

    CloudFeatures knownFeatures; //< Reported by the cloud or cached; the others take defaults.
    CloudFeatures enabledFeatures;
    std::string cacheUsername; //< Whose cache was loaded last.
    std::unique_ptr<nx::network::http::AsyncClient> httpClient;
    nx::network::http::Credentials lastUsedCredentials;
    int retryAttempts = 0;
    nx::utils::PendingOperation delayedRequestCloudFeatures{
        [this]() { requestCloudFeatures(); }, kRequestRetryDelay};

    ~Private();

    void stopHttpClient();
    void setFeatures(CloudFeatures known, CloudFeatures enabled);
    void loadCache();
    void storeCache(CloudFeatures enabled);
    void updateCredentials();
    void requestOperation();
    void requestCloudFeatures();
    void handleResponse();
};

CloudServiceChecker::Private::~Private()
{
    stopHttpClient();
}

void CloudServiceChecker::Private::stopHttpClient()
{
    if (!httpClient)
        return;

    httpClient->pleaseStopSync();
    httpClient.reset();
}

void CloudServiceChecker::Private::setFeatures(CloudFeatures known, CloudFeatures enabled)
{
    enabled &= known;
    if (knownFeatures == known && enabledFeatures == enabled)
        return;

    knownFeatures = known;
    enabledFeatures = enabled;
    emit q->featuresChanged();
}

void CloudServiceChecker::Private::loadCache()
{
    CloudFeatures lastKnownEnabledFeatures;
    cacheUsername = cloudStatusWatcher->credentials().username;
    auto cloudFeatures = settings->lastKnownCloudEnabledFeatures();
    if (cloudFeatures.contains(cacheUsername))
        ::fromString(cloudFeatures[cacheUsername], &lastKnownEnabledFeatures);

    // Only the enabled features are cached, the rest are not known.
    setFeatures(/*known*/ lastKnownEnabledFeatures, lastKnownEnabledFeatures);
}

void CloudServiceChecker::Private::storeCache(CloudFeatures enabled)
{
    auto cloudFeatures = settings->lastKnownCloudEnabledFeatures();
    const auto credentials = cloudStatusWatcher->credentials();
    const std::string currentFeatures = ::toString(enabled);

    if (cloudFeatures[credentials.username] == currentFeatures)
        return;

    cloudFeatures[credentials.username] = currentFeatures;
    settings->lastKnownCloudEnabledFeatures = cloudFeatures;
}

void CloudServiceChecker::Private::updateCredentials()
{
    const auto credentials = cloudStatusWatcher->credentials();
    lastUsedCredentials = credentials;
    retryAttempts = 0;
    // The disabled features are not cached, so the same user keeps them until the response.
    if (credentials.username != cacheUsername)
        loadCache();
    delayedRequestCloudFeatures.fire();
}

void CloudServiceChecker::Private::requestOperation()
{
    if (retryAttempts >= kMaxRetryAttempts)
    {
        NX_WARNING(this,
            "Maximum retry attempts (%1) reached for cloud features request",
            kMaxRetryAttempts);
        return;
    }
    ++retryAttempts;
    executeLater([this]() { delayedRequestCloudFeatures.requestOperation(); }, q);
}

void CloudServiceChecker::Private::requestCloudFeatures()
{
    // Some tests do not use network. It is OK to not initialize AsyncClient for them.
    if (!nx::network::SocketGlobals::isInitialized())
    {
        NX_VERBOSE(this, "SocketGlobals is not initialized, cannot get cloud feature flags");
        return;
    }

    stopHttpClient();

    const nx::Url urlCloud = cloudPortalUrl.isEmpty()
        ? nx::Url(nx::network::AppInfo::defaultCloudPortalUrl(
              nx::network::SocketGlobals::cloud().cloudHost()))
        : cloudPortalUrl;

    const auto address = nx::network::SocketAddress::fromUrl(urlCloud, true);

    httpClient =
        std::make_unique<nx::network::http::AsyncClient>(nx::network::ssl::kAcceptAnyCertificate);
    httpClient->setCredentials(lastUsedCredentials);

    nx::network::url::Builder url;
    url.setScheme(urlCloud.scheme().toStdString()).setEndpoint(address);
    url.setPath(kApiCloudSettingsPath);

    httpClient->doGet(url.toUrl(),
        nx::utils::AsyncHandlerExecutor(q).bind(
            [this, storedCredentials = lastUsedCredentials]()
            {
                if (storedCredentials == lastUsedCredentials)
                    handleResponse();
            }));
}

void CloudServiceChecker::Private::handleResponse()
{
    if (httpClient->failed())
    {
        NX_VERBOSE(this, "Failed to get cloud feature flags");
        requestOperation();
        return;
    }

    const auto buffer = httpClient->fetchMessageBodyBuffer();
    stopHttpClient();
    if (buffer.empty())
    {
        NX_VERBOSE(this, "Failed to fetch cloud feature flags");
        requestOperation();
        return;
    }

    QJsonParseError error;
    const QJsonDocument doc =
        QJsonDocument::fromJson(QByteArray::fromStdString(buffer.toString()), &error);
    if (error.error != QJsonParseError::NoError)
    {
        NX_ERROR(this, "Cannot parse cloud feature flags: %1", error.errorString());
        return;
    }

    if (!doc.isObject())
    {
        NX_ERROR(this, "Cloud feature flags response does not contain JSON object.");
        return;
    }

    CloudFeatures known;
    CloudFeatures enabled;
    CloudFeatures withdrawn; //< The features of missing services.
    const QJsonObject featureFlags = doc.object()[kFeatureFlagsKey].toObject();
    for (auto it = featureFlags.constBegin(); it != featureFlags.constEnd(); ++it)
    {
        if (!it.value().isBool())
            continue;

        const std::string name = it.key().toStdString();
        if (const auto service = fromStringStrict<CloudService>(name); service.has_value())
        {
            if (!it.value().toBool())
                withdrawn |= featuresOf(service.value());
            continue;
        }

        if (const auto feature = fromStringStrict<CloudFeature>(name); feature.has_value())
        {
            known.setFlag(feature.value());
            enabled.setFlag(feature.value(), it.value().toBool());
        }
    }

    // A missing service wins over the flags of its features.
    known |= withdrawn;
    enabled &= ~withdrawn;

    retryAttempts = 0;
    storeCache(enabled);
    setFeatures(known, enabled);
}

CloudServiceChecker::CloudServiceChecker(
    AbstractCloudStatusWatcher* cloudStatusWatcher, Settings* settings, QObject* parent):
    CloudServiceChecker(cloudStatusWatcher, settings, nx::Url(), parent)
{
}

CloudServiceChecker::CloudServiceChecker(AbstractCloudStatusWatcher* cloudStatusWatcher,
    Settings* settings,
    const nx::Url& cloudPortalUrl,
    QObject* parent):
    AbstractCloudFeatureSource(parent),
    d(new Private{.q = this,
        .cloudStatusWatcher = cloudStatusWatcher,
        .settings = settings,
        .cloudPortalUrl = cloudPortalUrl})
{
    if (!NX_ASSERT(cloudStatusWatcher && settings))
        return;

    connect(cloudStatusWatcher,
        &AbstractCloudStatusWatcher::cloudLoginChanged,
        this,
        [this]() { d->loadCache(); });

    connect(cloudStatusWatcher,
        &AbstractCloudStatusWatcher::credentialsChanged,
        this,
        [this]() { d->updateCredentials(); });

    d->loadCache();
    d->requestCloudFeatures();
}

CloudServiceChecker::~CloudServiceChecker()
{
}

CloudFeatures CloudServiceChecker::enabledFeatures() const
{
    return d->enabledFeatures | (kFeaturesEnabledByDefault & ~d->knownFeatures);
}

} // namespace nx::vms::client::core
