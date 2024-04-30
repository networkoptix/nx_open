// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client.h"

#include <nx/network/socket_delegate.h>
#include <nx/utils/log/log.h>

#include "detail/client_factory.h"

namespace nx::network::http::tunneling {

namespace {

/**
 * Reports absence of incoming data to the given functor during the destruction.
 */
class ReportingConnection:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    ReportingConnection(
        std::unique_ptr<AbstractStreamSocket> socket,
        detail::ClientFeedbackFunction feedbackFunc)
        :
        base_type(socket.get()),
        m_socket(std::move(socket)),
        m_feedbackFunc(std::move(feedbackFunc))
    {
    }

    ~ReportingConnection()
    {
        if (!m_isDataReceived && m_feedbackFunc)
            m_feedbackFunc(false);
    }

    virtual int recv(void* buffer, std::size_t bufferLen, int flags) override
    {
        const auto bytesReceived = base_type::recv(buffer, bufferLen, flags);
        if (!m_isDataReceived && bytesReceived > 0)
            provideSuccessFeedback();
        return bytesReceived;
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override
    {
        return this->m_target->readSomeAsync(
            buffer,
            [this, handler = std::move(handler)](
                SystemError::ErrorCode resultCode,
                std::size_t bytesReceived)
            {
                if (!m_isDataReceived && resultCode == SystemError::noError && bytesReceived > 0)
                    provideSuccessFeedback();
                handler(resultCode, bytesReceived);
            });
    }

private:
    void provideSuccessFeedback()
    {
        m_isDataReceived = true;
        if (m_feedbackFunc)
            nx::utils::swapAndCall(m_feedbackFunc, true);
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    bool m_isDataReceived = false;
    detail::ClientFeedbackFunction m_feedbackFunc;
};

} // namespace

//-------------------------------------------------------------------------------------------------

Client::Client(
    const nx::utils::Url& baseTunnelUrl,
    const std::string& userTag,
    std::optional<int> forcedTunnelType,
    const ConnectOptions& options)
    :
    m_baseTunnelUrl(baseTunnelUrl)
{
    auto clients = detail::ClientFactory::instance().create(
        userTag, baseTunnelUrl, forcedTunnelType, options);

    std::transform(
        clients.begin(), clients.end(),
        std::back_inserter(m_actualClients),
        [](auto& client)
        {
            ClientContext ctx;
            ctx.client = std::move(client);
            return ctx;
        });

    for (auto& ctx: m_actualClients)
        ctx.client->bindToAioThread(getAioThread());
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::setTunnelValidatorFactory(TunnelValidatorFactoryFunc func)
{
    m_validatorFactory = std::move(func);
}

void Client::setConsiderSilentConnectionAsTunnelFailure(bool value)
{
    m_isConsideringSilentConnectionATunnelFailure = value;
}

void Client::setConcurrentTunnelMethodsEnabled(bool value)
{
    m_concurrentTunnelMethodsEnabled = value;
}

void Client::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& ctx: m_actualClients)
    {
        if (ctx.client)
            ctx.client->bindToAioThread(aioThread);
        if (ctx.validator)
            ctx.validator->bindToAioThread(aioThread);
    }
}

void Client::setTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    m_timeout = timeout;

    for (auto& ctx: m_actualClients)
        ctx.client->setTimeout(timeout);
}

void Client::setCustomHeaders(HttpHeaders headers)
{
    for (auto& ctx: m_actualClients)
        ctx.client->setCustomHeaders(headers);
}

void Client::openTunnel(
    OpenTunnelCompletionHandler completionHandler)
{
    NX_VERBOSE(this, "Opening tunnel to %1 with %2 concurrent methods",
        m_baseTunnelUrl, m_actualClients.size());

    m_completionHandler = std::move(completionHandler);

    if (!m_concurrentTunnelMethodsEnabled && !m_actualClients.empty())
        m_actualClients.erase(++m_actualClients.begin(), m_actualClients.end());

    for (auto& ctx: m_actualClients)
    {
        ctx.client->openTunnel(
            [this, ctx = &ctx](auto&&... args)
            {
                handleOpenTunnelCompletion(
                    ctx, std::forward<decltype(args)>(args)...);
            });
    }
}

const Response& Client::response() const
{
    return m_response;
}

void Client::stopWhileInAioThread()
{
    m_actualClients.clear();
}

void Client::handleOpenTunnelCompletion(ClientContext* ctx, OpenTunnelResult result)
{
    if (result.ok())
        ctx->response = ctx->client->response();

    if (!result.ok() || !m_validatorFactory)
    {
        ++m_completedClients;
        if (!result.ok() && m_completedClients < m_actualClients.size())
            return; //< Waiting for other clients to complete.
        return reportResult(ctx, std::move(result));
    }

    NX_VERBOSE(this, "Validating tunnel %1 to %2", result.connection.get(), m_baseTunnelUrl);

    ctx->validator = m_validatorFactory(
        std::exchange(result.connection, nullptr),
        ctx->response);
    ctx->validator->bindToAioThread(getAioThread());
    if (m_timeout)
        ctx->validator->setTimeout(*m_timeout);
    ctx->validator->validate(
        [this, ctx](ResultCode result) { handleTunnelValidationResult(ctx, result); });

    ctx->result = std::move(result);
}

void Client::handleTunnelValidationResult(ClientContext* ctx, ResultCode resultCode)
{
    auto validator = std::exchange(ctx->validator, nullptr);

    if (resultCode != ResultCode::ok)
    {
        // Resetting previously saved result since it is cancelled by the validation failure.
        ctx->result = OpenTunnelResult();
        ctx->result.resultCode = resultCode;
    }
    // else keeping the result from the tunnel connect.

    if (ctx->result.resultCode == ResultCode::ok)
        ctx->result.connection = validator->takeConnection();
    else
        ctx->client->takeFeedbackFunction()(false);

    NX_VERBOSE(this, "Validation of tunnel %1 to %2 completed with result %3",
        ctx->result.connection ? ctx->result.connection.get() : nullptr,
        m_baseTunnelUrl, toString(resultCode));

    ++m_completedClients;
    if (resultCode != ResultCode::ok && m_completedClients < m_actualClients.size())
        return; //< Waiting for other clients to complete.

    reportResult(ctx, std::exchange(ctx->result, {}));
}

void Client::reportResult(ClientContext* ctx, OpenTunnelResult result)
{
    if (result.ok())
    {
        if (m_isConsideringSilentConnectionATunnelFailure)
        {
            result.connection = std::make_unique<ReportingConnection>(
                std::exchange(result.connection, nullptr),
                ctx->client->takeFeedbackFunction());
        }
        else
        {
            if (auto func = ctx->client->takeFeedbackFunction(); func)
                func(true);
        }
    }

    m_response = std::exchange(ctx->response, {});

    m_actualClients.clear();

    return nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

} // namespace nx::network::http::tunneling
