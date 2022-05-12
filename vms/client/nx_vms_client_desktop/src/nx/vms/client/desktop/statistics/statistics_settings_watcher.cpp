// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "statistics_settings_watcher.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/statistics/settings.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

struct StatisticsSettingsWatcher::Private: public QObject
{
    StatisticsSettingsWatcher* const q;
    std::optional<QnStatisticsSettings> settings;
    std::unique_ptr<nx::network::http::AsyncClient> request;
    mutable nx::Mutex mutex;

    Private(StatisticsSettingsWatcher* owner):
        q(owner)
    {
    }

    ~Private()
    {
        if (request)
            request->pleaseStopSync();
    }

    nx::utils::Url actualUrl() const
    {
        const auto localSettingsUrl =
            q->globalSettings()->clientStatisticsSettingsUrl();

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

StatisticsSettingsWatcher::StatisticsSettingsWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

StatisticsSettingsWatcher::~StatisticsSettingsWatcher()
{}

std::optional<QnStatisticsSettings> StatisticsSettingsWatcher::settings() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->settings;
}

void StatisticsSettingsWatcher::updateSettings()
{
    d->updateSettings();
}

} // namespace nx::vms::client::desktop
