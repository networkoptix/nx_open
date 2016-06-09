
#pragma once

#include <memory>

#include <cdb/connection.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/singleton.h>


class CloudConnectionManager;

class CloudSystemNameUpdater
:
    public Singleton<CloudSystemNameUpdater>
{
public:
    CloudSystemNameUpdater(CloudConnectionManager* const cloudConnectionManager);
    virtual ~CloudSystemNameUpdater();

    /** Push system name to the cloud ASAP */
    void update();

private:
    CloudConnectionManager* const m_cloudConnectionManager;
    nx::utils::AtomicUniquePtr<nx::cdb::api::Connection> m_cloudConnection;
    QnMutex m_mutex;
    bool m_terminated;
    nx::utils::TimerId m_timerId;

    void scheduleNextUpdate(std::chrono::seconds delay);
    void updateSystemNameAsync();
    void systemNameUpdated(nx::cdb::api::ResultCode resultCode);
};
