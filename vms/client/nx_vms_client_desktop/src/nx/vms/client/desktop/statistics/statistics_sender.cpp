// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "statistics_sender.h"

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <statistics/statistics_manager.h>

#include "statistics_settings_watcher.h"

namespace nx::vms::client::desktop {

struct StatisticsSender::Private
{
    StatisticsSender* const q;
    std::unique_ptr<StatisticsSettingsWatcher> settingsWatcher;

    bool canSendStatistics() const
    {
        // Exclude developers from the statisics.
        if (nx::build_info::publicationType() == nx::build_info::PublicationType::local)
            return false;

        if (!settingsWatcher->settings())
            return false;

        return q->systemSettings()->isInitialized() && q->systemSettings()->isStatisticsAllowed()
            && !q->systemSettings()->isNewSystem();
    }

    void sendStatistics()
    {
        if (!canSendStatistics())
            return;

        auto server = q->systemContext()->currentServer();

        if (!server || !server->hasInternetAccess())
            server = q->systemContext()->resourcePool()->serverWithInternetAccess();

        if (!server)
            return;

        auto manager = statisticsModule()->manager();
        const auto statisticsData = manager->prepareStatisticsData(*settingsWatcher->settings());
        if (!statisticsData)
            return;

        auto callback = nx::utils::guarded(q,
            [statisticsData](
                bool success,
                rest::Handle /*handle*/,
                rest::ServerConnection::EmptyResponseType /*response */)
            {
                if (!success)
                    return;

                statisticsModule()->manager()->saveSentStatisticsData(
                    statisticsData->timestamp,
                    statisticsData->filters);
            });

        q->systemContext()->connectedServerApi()->sendStatisticsUsingServer(
            server->getId(),
            statisticsData->payload,
            callback,
            q->thread());
    }
};

StatisticsSender::StatisticsSender(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q = this})
{
    d->settingsWatcher = std::make_unique<StatisticsSettingsWatcher>(systemContext);

    connect(d->settingsWatcher.get(),
        &StatisticsSettingsWatcher::settingsAvailableChanged,
        this,
        [this]
        {
            d->sendStatistics();
        });
}

StatisticsSender::~StatisticsSender()
{
}

} // namespace nx::vms::client::desktop
