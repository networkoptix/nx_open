// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/time_sync/time_sync_manager.h>

#include <nx/utils/elapsed_timer.h>

namespace nx::vms::client::core {

class TimeSyncManager: public nx::vms::time::TimeSyncManager
{
    Q_OBJECT
    using base_type = nx::vms::time::TimeSyncManager;
public:
    TimeSyncManager(QnCommonModule* commonModule);
    virtual ~TimeSyncManager() override;

    virtual void resync() override;

protected:
    virtual void updateTime() override;

private:
    nx::utils::ElapsedTimer m_lastSyncTimeInterval;
};

} // namespace nx::vms::client::core
