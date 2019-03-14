#include "remote_relay_peer_pool_aio_wrapper.h"

namespace nx::cloud::relay::model {

RemoteRelayPeerPoolAioWrapper::RemoteRelayPeerPoolAioWrapper(
    const AbstractRemoteRelayPeerPool& remoteRelayPeerPool)
    :
    m_remoteRelayPeerPool(remoteRelayPeerPool)
{
}

RemoteRelayPeerPoolAioWrapper::~RemoteRelayPeerPoolAioWrapper()
{
    m_pendingAsyncCallCounter.wait();

    m_asyncCallProvider.pleaseStopSync();
}

bool RemoteRelayPeerPoolAioWrapper::isConnected() const
{
    return m_remoteRelayPeerPool.isConnected();
}

void RemoteRelayPeerPoolAioWrapper::findRelayByDomain(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(std::string /*relay host*/)> completionHandler)
{
    m_remoteRelayPeerPool.findRelayByDomain(domainName).then(
        [this, lock = m_pendingAsyncCallCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                cf::future<std::string> relayDomainFuture) mutable
        {
            m_asyncCallProvider.post(
                [completionHandler = std::move(completionHandler),
                    result = relayDomainFuture.get()]() mutable
                {
                    completionHandler(std::move(result));
                });

            return cf::unit();
        });
}

} // namespace nx::cloud::relay::model
