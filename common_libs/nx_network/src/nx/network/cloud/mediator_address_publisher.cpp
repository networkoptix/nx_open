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
            NX_LOGX(lm("Mediator client reported reconnect"), cl_logDEBUG2);
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
            m_serverAddresses = std::move(addresses);
            if (handler)
                m_updateHandlers.push_back(std::move(handler));
            NX_LOGX(lm("New addresses: %1").container(m_serverAddresses), cl_logDEBUG1);
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::publishAddressesIfNeeded()
{
    if (m_publishedAddresses == m_serverAddresses)
    {
        NX_LOGX(lm("No need to publish addresses: they are already published. Reporting success..."),
            cl_logDEBUG2);
        reportResultToTheCaller(hpm::api::ResultCode::ok);
        return;
    }

    if (m_isRequestInProgress)
    {
        NX_LOGX(lm("Publish address request has already been issued. Ignoring new one..."), cl_logDEBUG2);
        return;
    }

    NX_LOGX(lm("Issuing bind request to mediator..."), cl_logDEBUG2);

    m_isRequestInProgress = true;
    m_mediatorConnection->bind(
        nx::hpm::api::BindRequest(m_serverAddresses),
        [this, addresses = m_serverAddresses](nx::hpm::api::ResultCode resultCode)
        {
            NX_LOGX(lm("Publish addresses (%1) completed with result %2")
                .container(m_publishedAddresses).arg(resultCode), cl_logDEBUG1);

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
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::stopWhileInAioThread()
{
    m_mediatorConnection.reset();
}

void MediatorAddressPublisher::reportResultToTheCaller(hpm::api::ResultCode resultCode)
{
    if (m_updateHandlers.empty())
        return;

    decltype(m_updateHandlers) handlers;
    std::swap(handlers, m_updateHandlers);

    for (auto& handler: handlers)
        handler(resultCode);
}

} // namespace cloud
} // namespace network
} // namespace nx
