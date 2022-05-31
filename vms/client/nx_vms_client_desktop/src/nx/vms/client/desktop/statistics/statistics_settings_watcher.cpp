// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "statistics_settings_watcher.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/statistics/settings.h>
#include <utils/common/delayed.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

constexpr auto kUpdateSettingsInterval = 4h;

} // namespace

struct StatisticsSettingsWatcher::Private
{
    StatisticsSettingsWatcher* const q;
    std::optional<QnStatisticsSettings> settings;
    std::unique_ptr<nx::network::http::AsyncClient> request;
    mutable nx::Mutex mutex;

    ~Private()
    {
        if (request)
            request->pleaseStopSync();
    }

    nx::utils::Url actualUrl() const
    {
        const auto localSettingsUrl =
            q->systemSettings()->clientStatisticsSettingsUrl();

        if (localSettingsUrl.isValid())
            return localSettingsUrl;

        if (nx::vms::statistics::kDefaultStatisticsServer.isEmpty())
            return {};

        return nx::vms::statistics::kDefaultStatisticsServer + "/config/client_stats_v2.json";
    }

    void updateSettings()
    {
        if (request) //< Do nothing if request is already in progress.
            return;

        const auto url = actualUrl();
        if (!url.isValid())
            return;

        request = std::make_unique<nx::network::http::AsyncClient>(
            nx::network::ssl::kAcceptAnyCertificate);
        core::NetworkManager::setDefaultTimeouts(request.get());

        auto onHttpDone =
            [this]()
            {
                if (!request->failed() && request->response())
                {
                    nx::Buffer messageBody = request->fetchMessageBodyBuffer();
                    QnStatisticsSettings settings;
                    bool success = QJson::deserialize(messageBody.toByteArray(), &settings);
                    if (success)
                        setSettings(settings);
                    else
                        resetSettings();
                }
                else
                {
                    resetSettings();
                }
                request.reset();
            };

        request->setOnDone(onHttpDone);
        request->doGet(url);
    }

    void resetSettings()
    {
        {
            NX_MUTEX_LOCKER lock(&mutex);
            if (!settings)
                return;

            settings.reset();
        }
        emit q->settingsAvailableChanged();
    }

    void setSettings(const QnStatisticsSettings& value)
    {
        {
            NX_MUTEX_LOCKER lock(&mutex);
            const bool sameSettings = (settings && (*settings == value));
            if (sameSettings)
                return;

            settings = value;
        }
        emit q->settingsAvailableChanged();
    }
};

StatisticsSettingsWatcher::StatisticsSettingsWatcher(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q = this})
{
    auto updateSettings = [this] { d->updateSettings(); };

    connect(systemSettings(),
        &common::SystemSettings::initialized,
        this,
        updateSettings);

    auto updateSettingsTimer = new QTimer(this);
    updateSettingsTimer->setSingleShot(false);
    updateSettingsTimer->setInterval(kUpdateSettingsInterval);
    updateSettingsTimer->callOnTimeout(updateSettings);
    updateSettingsTimer->start();
}

StatisticsSettingsWatcher::~StatisticsSettingsWatcher()
{
}

std::optional<QnStatisticsSettings> StatisticsSettingsWatcher::settings() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->settings;
}

} // namespace nx::vms::client::desktop
