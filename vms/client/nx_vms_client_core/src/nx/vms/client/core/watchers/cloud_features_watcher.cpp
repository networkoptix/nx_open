// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_features_watcher.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

#include <nx/branding.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/from_string.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/json/flags.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std_string_utils.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <utils/common/delayed.h>

namespace {

static const std::chrono::seconds kRequestRetryDelay(120);
static const int kMaxRetryAttempts = 3;
static const QString kApiCloudSettingsPath = "/api/utils/settings";
static const QString kFeatureFlagsKey = "featureFlags";

nx::vms::client::core::CloudFeature fromString(std::string_view cloudFeatureName)
{
    using namespace nx::vms::client::core;
    return nx::reflect::fromString<CloudFeature>(cloudFeatureName, CloudFeature::none);
}

} // namespace

namespace nx::vms::client::core {

struct CloudFeaturesWatcher::Private
{
    Private(CloudFeaturesWatcher* owner):
        q(owner)
    {
        const std::string_view disabledCloudFeaturesString(ini().forcefullyDisabledFeatures);
        nx::utils::split(
            disabledCloudFeaturesString,
            nx::utils::separator::isAnyOf(" ,;"),
            [this](std::string_view serviceName)
            {
                if (const auto service = fromString(serviceName); service != CloudFeature::none)
                    forcefullyDisabledFeatures.setFlag(service);
            },
            nx::utils::GroupToken::none,
            nx::utils::SplitterFlag::skipEmpty);

        connect(
            appContext()->cloudStatusWatcher(),
            &CloudStatusWatcher::credentialsChanged,
            q,
            [this]()
            {
                CloudFeaturesWatcher::CloudFeatures lastKnownEnabledFeatures;
                const auto credentials = appContext()->cloudStatusWatcher()->credentials();
                auto cloudFeatures = appContext()->coreSettings()->lastKnownCloudEnabledFeatures();
                if (cloudFeatures.contains(credentials.username))
                    fromString(cloudFeatures[credentials.username], &lastKnownEnabledFeatures);

                cloudEnabledFeatures = lastKnownEnabledFeatures;
                lastUsedCredentials = credentials;
                retryAttempts = 0;
                updateEnabledFeatures();
                delayedRequestCloudFeatures.fire();
            });

        updateEnabledFeatures();
        requestCloudFeatures();
    }

    ~Private() { stopHttpClient(); }

    CloudFeaturesWatcher* const q = nullptr;

    CloudFeaturesWatcher::CloudFeatures enabledFeatures;
    CloudFeaturesWatcher::CloudFeatures cloudEnabledFeatures;
    CloudFeaturesWatcher::CloudFeatures forcefullyDisabledFeatures;
    std::unique_ptr<nx::network::http::AsyncClient> httpClient;
    nx::network::http::Credentials lastUsedCredentials;
    int retryAttempts = 0;
    nx::utils::PendingOperation delayedRequestCloudFeatures{
        [this]()
        {
            requestCloudFeatures();
        }, kRequestRetryDelay};

    void stopHttpClient()
    {
        if (!httpClient)
            return;

        httpClient->pleaseStopSync();
        httpClient.reset();
    }

    void updateLastKnownCloudEnabledFeaturesSettings()
    {
        auto cloudFeatures = appContext()->coreSettings()->lastKnownCloudEnabledFeatures();
        const auto credentials = appContext()->cloudStatusWatcher()->credentials();
        const std::string currentFeatures = ::toString(cloudEnabledFeatures);

        if (cloudFeatures[credentials.username] == currentFeatures)
            return;

        cloudFeatures[credentials.username] = currentFeatures;
        appContext()->coreSettings()->lastKnownCloudEnabledFeatures = cloudFeatures;
    };

    void updateEnabledFeatures()
    {
        auto features = cloudEnabledFeatures & (~forcefullyDisabledFeatures);
        if (enabledFeatures == features)
            return;

        const auto changedFeatures = enabledFeatures ^ features;
        enabledFeatures = features;
        updateLastKnownCloudEnabledFeaturesSettings();
        emit q->featuresChanged(changedFeatures);
    }

    void requestOperation()
    {
        if (retryAttempts >= kMaxRetryAttempts)
        {
          NX_WARNING(this, "Maximum retry attempts (%1) reached for cloud features request",
              kMaxRetryAttempts);
          return;
        }
        ++retryAttempts;
        executeLater([this](){ delayedRequestCloudFeatures.requestOperation(); }, q);
    }

    void requestCloudFeatures()
    {
        // Some tests do not use network. It is OK to not intialize AsyncClient for them.
        if (!nx::network::SocketGlobals::isInitialized())
        {
            NX_VERBOSE(this, "SocketGlobals is not initialized, cannot get cloud feature flags");
            return;
        }

        stopHttpClient();

        nx::Url urlCloud{nx::network::AppInfo::defaultCloudPortalUrl(
            nx::network::SocketGlobals::cloud().cloudHost())};

        const auto address = nx::network::SocketAddress::fromUrl(urlCloud, true);

        httpClient = std::make_unique<nx::network::http::AsyncClient>(
            nx::network::ssl::kAcceptAnyCertificate);
        httpClient->setCredentials(lastUsedCredentials);

        const auto url = nx::network::url::Builder()
            .setScheme(urlCloud.scheme().toStdString())
            .setEndpoint(address)
            .setPath(kApiCloudSettingsPath)
            .toUrl();

        auto str = url.toString().toStdString();

        auto callback = nx::utils::AsyncHandlerExecutor(q).bind(
            [this, storedCredentials = lastUsedCredentials] {
            if (storedCredentials != lastUsedCredentials)
                return;

            const nx::utils::ScopeGuard guard([this]() { updateEnabledFeatures(); });
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
            QJsonDocument doc =
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

            CloudFeaturesWatcher::CloudFeatures features;

            QJsonObject featureFlags = doc.object()[kFeatureFlagsKey].toObject();
            for (const auto& key : featureFlags.keys())
            {
                const auto feature = fromString(key.toStdString());
                if (feature != CloudFeature::none && featureFlags[key].toBool())
                    features |= feature;
            }
            cloudEnabledFeatures = features;
            retryAttempts = 0;
        });

        httpClient->doGet(url, std::move(callback));
    }
};

CloudFeaturesWatcher::CloudFeaturesWatcher(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
}

CloudFeaturesWatcher::~CloudFeaturesWatcher()
{
}

bool CloudFeaturesWatcher::hasFeature(CloudFeature feature) const
{
    return d->enabledFeatures.testFlag(feature);
}

} // namespace  nx::vms::client::core
