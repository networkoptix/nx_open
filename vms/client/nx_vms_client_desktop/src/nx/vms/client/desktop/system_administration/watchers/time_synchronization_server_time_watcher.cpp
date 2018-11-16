#include "time_synchronization_server_time_watcher.h"
#include "../redux/time_synchronization_widget_store.h"

#include <QtCore/QPointer>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop
{

class TimeSynchronizationServerTimeWatcher::Private : public QObject
{
    TimeSynchronizationServerTimeWatcher* q = nullptr;

public:
    Private(TimeSynchronizationServerTimeWatcher* q, TimeSynchronizationWidgetStore* store) :
        QObject(),
        q(q),
        m_store(store),
        m_currentRequest(rest::Handle())
    {
        NX_ASSERT(store);
    }

    void updateTimestamps()
    {
        if (m_currentRequest)
            return;

        const auto server = q->commonModule()->currentServer();
        if (!server)
            return;
        auto apiConnection = server->restConnection();

        const auto callback = nx::utils::guarded(this,
            [this, apiConnection]
            (bool success, rest::Handle handle, rest::MultiServerTimeData data)
            {
                if (m_currentRequest != handle)
                    return;

                m_currentRequest = rest::Handle(); //< Reset.
                if (!success)
                    return;

                const auto syncTime = qnSyncTime->currentMSecsSinceEpoch();
                TimeSynchronizationWidgetStore::TimeOffsetInfoList offsetList;

                for (const auto& record: data.data)
                {
                    TimeSynchronizationWidgetStore::TimeOffsetInfo offsetInfo;
                    offsetInfo.serverId = record.serverId;
                    offsetInfo.osTimeOffset = record.timeSinceEpochMs - syncTime;
                    offsetInfo.vmsTimeOffset = 0; // FIXME: use real value
                    offsetList += offsetInfo;
                }

                m_store->setTimeOffsets(offsetList);
            });

        m_currentRequest = apiConnection->getTimeOfServersAsync(callback, thread());
    }
private:
    QPointer<TimeSynchronizationWidgetStore> m_store;
    rest::Handle m_currentRequest;
    qint64 m_currentRequestStartTime;
};

TimeSynchronizationServerTimeWatcher::TimeSynchronizationServerTimeWatcher(
    TimeSynchronizationWidgetStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new Private(this, store))
{
}
    
void TimeSynchronizationServerTimeWatcher::updateTimestamps()
{
    d->updateTimestamps();
}

} // namespace nx::vms::client::desktop
