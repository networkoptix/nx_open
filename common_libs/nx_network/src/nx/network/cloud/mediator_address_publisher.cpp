#include "mediator_address_publisher.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

const std::chrono::milliseconds MediatorAddressPublisher::kDefaultRetryInterval =
    std::chrono::minutes(10);

MediatorAddressPublisher::MediatorAddressPublisher(
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection)
:
    m_retryInterval(kDefaultRetryInterval),
    m_isRequestInProgress(false),
    m_mediatorConnection(std::move(mediatorConnection))
{
    bindToAioThread(m_mediatorConnection->getAioThread());

    m_mediatorConnection->setOnReconnectedHandler(
        [this]()
        {
            m_publishedAddresses.clear();
            publishAddressesIfNeeded();
        });
}

MediatorAddressPublisher::~MediatorAddressPublisher()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void MediatorAddressPublisher::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);
    m_mediatorConnection->bindToAioThread(aioThread);
}

void MediatorAddressPublisher::setRetryInterval(std::chrono::milliseconds interval)
{
    m_mediatorConnection->dispatch([this, interval](){ m_retryInterval = interval; });
}

void MediatorAddressPublisher::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorConnection->pleaseStop(std::move(handler));
}

void MediatorAddressPublisher::updateAddresses(
    std::list<SocketAddress> addresses,
    utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> updateHandler)
{
    m_mediatorConnection->dispatch(
        [this, addresses = std::move(addresses), handler = std::move(updateHandler)]() mutable
        {
            NX_ASSERT(!m_updateHandler);
        
            m_serverAddresses = std::move(addresses);
            m_updateHandler = std::move(handler);
            NX_LOGX(lm("New addresses: %1").container(m_serverAddresses), cl_logDEBUG1);
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::publishAddressesIfNeeded()
{
    if (m_publishedAddresses == m_serverAddresses)
    {
        reportResultToTheCaller(hpm::api::ResultCode::ok);
        return;
    }

    if (m_isRequestInProgress)
        return;

    m_isRequestInProgress = true;
    m_mediatorConnection->bind(
        nx::hpm::api::BindRequest(m_serverAddresses),
        [this, addresses = m_serverAddresses](nx::hpm::api::ResultCode resultCode)
        {
            m_isRequestInProgress = false;

            reportResultToTheCaller(resultCode);

            if (resultCode != nx::hpm::api::ResultCode::ok)
            {
                m_publishedAddresses.clear();
                return m_mediatorConnection->start(
                    m_retryInterval,
                    [this](){ publishAddressesIfNeeded(); });
            }

            m_publishedAddresses = addresses;
            NX_LOGX(lm("Published addresses: %1").container(m_publishedAddresses), cl_logDEBUG1 );
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::stopWhileInAioThread()
{
    m_mediatorConnection.reset();
}

void MediatorAddressPublisher::reportResultToTheCaller(hpm::api::ResultCode resultCode)
{
    if (!m_updateHandler)
        return;

    decltype(m_updateHandler) handler;
    std::swap(handler, m_updateHandler);
    handler(resultCode);
}

} // namespace cloud
} // namespace network
} // namespace nx
