#include "client.h"

#include <nx/utils/log/log.h>

#include "detail/client_factory.h"

namespace nx::network::http::tunneling {

Client::Client(
    const nx::utils::Url& baseTunnelUrl,
    const std::string& userTag)
    :
    m_baseTunnelUrl(baseTunnelUrl),
    m_actualClient(detail::ClientFactory::instance().create(userTag, baseTunnelUrl))
{
    m_actualClient->bindToAioThread(getAioThread());
}

void Client::setTunnelValidatorFactory(TunnelValidatorFactoryFunc func)
{
    m_validatorFactory = std::move(func);
}

void Client::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_actualClient)
        m_actualClient->bindToAioThread(aioThread);
    if (m_validator)
        m_validator->bindToAioThread(aioThread);
}

void Client::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
    m_actualClient->setTimeout(timeout);
}

void Client::setCustomHeaders(HttpHeaders headers)
{
    m_actualClient->setCustomHeaders(std::move(headers));
}

void Client::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    m_completionHandler = std::move(completionHandler);

    m_actualClient->openTunnel(
        [this](auto&&... args)
        {
            handleOpenTunnelCompletion(std::forward<decltype(args)>(args)...);
        });
}

const Response& Client::response() const
{
    return m_actualClient->response();
}

void Client::stopWhileInAioThread()
{
    m_actualClient.reset();
    m_validator.reset();
}

void Client::handleOpenTunnelCompletion(OpenTunnelResult result)
{
    if (result.resultCode != ResultCode::ok || !m_validatorFactory)
        return nx::utils::swapAndCall(m_completionHandler, std::move(result));

    NX_VERBOSE(this, "Validating tunnel to %1", m_baseTunnelUrl);

    m_validator = m_validatorFactory(std::exchange(result.connection, nullptr));
    m_validator->bindToAioThread(getAioThread());
    if (m_timeout)
        m_validator->setTimeout(*m_timeout);
    m_validator->validate(
        [this](ResultCode result) { handleTunnelValidationResult(result); });

    m_lastResult = std::move(result);
}

void Client::handleTunnelValidationResult(ResultCode result)
{
    NX_VERBOSE(this, "Validation of tunnel to %1 completed with result %2",
        m_baseTunnelUrl, toString(result));

    auto validator = std::exchange(m_validator, nullptr);

    m_lastResult.resultCode = result;
    if (m_lastResult.resultCode == ResultCode::ok)
        m_lastResult.connection = validator->takeConnection();
    else
        m_actualClient->takeFeedbackFunction()(false);
    nx::utils::swapAndCall(m_completionHandler, std::exchange(m_lastResult, {}));
}

} // namespace nx::network::http::tunneling
