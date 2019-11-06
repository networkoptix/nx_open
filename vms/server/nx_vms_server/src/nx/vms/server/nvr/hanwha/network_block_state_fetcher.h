#pragma once

#include <functional>

#include <nx/utils/thread/long_runnable.h>

#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockStateFetcher: public QnLongRunnable
{
public:
    NetworkBlockStateFetcher(
        std::function<void(const nx::vms::api::NetworkBlockData& state)> stateHandler);

    virtual ~NetworkBlockStateFetcher();

protected:
    virtual void run() override;

private:
    std::function<void(const nx::vms::api::NetworkBlockData& state)> m_stateHandler;
};

} // namespace nx::vms::server::nvr::hanwha
