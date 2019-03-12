#include "mediator_address_publisher.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

const std::chrono::milliseconds MediatorAddressPublisher::kDefaultRetryInterval =
    std::chrono::minutes(1);

static constexpr auto kCheckCloudCredentialsAvailabilityPeriod = std::chrono::seconds(17);

MediatorAddressPublisher::MediatorAddressPublisher(
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
    hpm::api::AbstractCloudSystemCredentialsProvider* cloudCredentialsProvider)
:
    m_retryInterval(kDefaultRetryInterval),
    m_isRequestInProgress(false),
    m_mediatorConnection(std::move(mediatorConnection)),
    m_cloudCredentialsProvider(cloudCredentialsProvider)
{
    bindToAioThread(m_mediatorConnection->getAioThread());

    m_mediatorConnection->setOnReconnectedHandler(
        [this]()
        {
            NX_VERBOSE(this, lm("Mediator client reported reconnect"));
            m_publishedAddresses.clear();
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_mediatorConnection->bindToAioThread(aioThread);
    if (m_retryTimer)
        m_retryTimer->bindToAioThread(aioThread);
}

void MediatorAddressPublisher::setRetryInterval(std::chrono::milliseconds interval)
{
    m_mediatorConnection->dispatch([this, interval](){ m_retryInterval = interval; });
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
            NX_DEBUG(this, lm("New addresses: %1").container(m_serverAddresses));
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::publishAddressesIfNeeded()
{
    NX_ASSERT(isInSelfAioThread());

    if (m_publishedAddresses == m_serverAddresses)
    {
        NX_VERBOSE(this, lm("No need to publish addresses: they are already published. Reporting success..."));
        reportResultToTheCaller(hpm::api::ResultCode::ok);
        return;
    }

    if (m_isRequestInProgress)
    {
        NX_VERBOSE(this, lm("Publish address request has already been issued. Ignoring new one..."));
        return;
    }

    m_isRequestInProgress = true;

    if (m_cloudCredentialsProvider->getSystemCredentials())
        registerAddressesOnMediator();
    else
        scheduleRetry(kCheckCloudCredentialsAvailabilityPeriod);
}

void MediatorAddressPublisher::registerAddressesOnMediator()
{
    NX_VERBOSE(this, lm("Issuing bind request to mediator..."));

    m_mediatorConnection->bind(
        nx::hpm::api::BindRequest(m_serverAddresses),
        [this, addresses = m_serverAddresses](nx::hpm::api::ResultCode resultCode)
        {
            NX_DEBUG(this, lm("Publish addresses (%1) completed with result %2")
                .container(m_publishedAddresses).arg(resultCode));

            m_isRequestInProgress = false;

            reportResultToTheCaller(resultCode);

            if (resultCode != nx::hpm::api::ResultCode::ok)
            {
                m_publishedAddresses.clear();
                scheduleRetry(m_retryInterval);
                return;
            }

            m_publishedAddresses = addresses;
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::scheduleRetry(
    std::chrono::milliseconds delay)
{
    m_retryTimer = std::make_unique<nx::network::aio::Timer>();
    m_retryTimer->start(
        delay,
        [this]()
        {
            m_retryTimer.reset();
            publishAddressesIfNeeded();
        });
}

void MediatorAddressPublisher::stopWhileInAioThread()
{
    m_mediatorConnection.reset();
    m_retryTimer.reset();
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
