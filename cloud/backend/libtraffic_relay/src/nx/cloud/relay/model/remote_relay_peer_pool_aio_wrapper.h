#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/counter.h>

#include "abstract_remote_relay_peer_pool.h"

namespace nx::cloud::relay::model {

/**
 * Delegates calls to AbstractRemoteRelayPeerPool instance
 * and delivers result to a completionHandler within an aio thread.
 */
class RemoteRelayPeerPoolAioWrapper
{
public:
    RemoteRelayPeerPoolAioWrapper(
        const AbstractRemoteRelayPeerPool& remoteRelayPeerPool);
    /**
     * Blocks until every request is processed.
     */
    virtual ~RemoteRelayPeerPoolAioWrapper();

    bool isConnected() const;

    void findRelayByDomain(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(std::string /*relay host*/)> completionHandler);

private:
    const AbstractRemoteRelayPeerPool& m_remoteRelayPeerPool;
    nx::network::aio::BasicPollable m_asyncCallProvider;
    nx::utils::Counter m_pendingAsyncCallCounter;
};

} // namespace nx::cloud::relay::model
