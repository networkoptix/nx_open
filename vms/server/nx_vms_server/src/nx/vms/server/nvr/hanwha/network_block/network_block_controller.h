#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockStateFetcher;
class INetworkBlockPlatformAbstraction;

class NetworkBlockController
{
public:
    using StateHandler = std::function<void(const NetworkPortStateList&)>;
public:
    NetworkBlockController(
        std::unique_ptr<INetworkBlockPlatformAbstraction> platformAbstraction,
        StateHandler stateHandler);

    virtual ~NetworkBlockController();

    void start();

    void stop();

    NetworkPortStateList state() const;

    NetworkPortPoeStateList setPoeStates(const NetworkPortPoeStateList& poeStates);

private:
    void updateStates(const NetworkPortStateList& states);

private:
    mutable QnMutex m_mutex;

    std::unique_ptr<INetworkBlockPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<NetworkBlockStateFetcher> m_stateFetcher;
    NetworkPortStateList m_lastPortStates;
    StateHandler m_handler;
};

} // namespace nx::vms::server::nvr::hanwha
