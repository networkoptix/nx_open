#include "websocket.h"

#include <exception>
#include <nx/network/websocket/websocket_handshake.h>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

WebSocket::WebSocket()
{
}

WebSocket::~WebSocket()
{
    pleaseStopSync();
}

cf::future<cf::unit> WebSocket::connect(nx::utils::Url url)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, url = std::move(url)](auto future) mutable
        {
            future.get();

            close();

            http::HttpHeaders headers;
            websocket::addClientHeaders(&headers,
                "tracker-protocol", websocket::CompressionType::perMessageDeflate);
            m_httpClient.addAdditionalRequestHeaders(headers);

            return m_httpClient.get(std::move(url));
        })
    .then(
        [this](auto future)
        {
            const auto response = future.get();

            const auto error = websocket::validateResponse(
                m_httpClient.request(), response);
            if (error != websocket::Error::noError)
                throw std::runtime_error("Handshake failed");

            m_nested.emplace(
                m_httpClient.takeSocket(),
                websocket::Role::client,
                websocket::FrameType::text,
                websocket::compressionType(response.headers));

            m_nested->start();

            return cf::unit();
        })
    .then(
        [](auto future)
        {
            try
            {
                return future.get();
            }
            catch (...)
            {
                std::throw_with_nested(std::runtime_error(
                    "Failed to connect websocket"));
            }
        });
}

cf::future<nx::Buffer> WebSocket::read()
{
    return cf::make_ready_future(cf::unit()).then(
        [this](auto upFuture)
        {
            upFuture.get();

            cf::promise<SystemError::ErrorCode> promise;
            auto future = treatBrokenPromiseAsCancelled(promise.get_future());

            m_buffer.clear();
            m_nested->readSomeAsync(&m_buffer,
                [promise = std::move(promise)](auto errorCode, auto /*size*/) mutable
                {
                    promise.set_value(errorCode);
                });

            return future;
        })
    .then(
        [this](auto future)
        {
            try
            {
                const auto errorCode = future.get();
                if (errorCode)
                    throw std::system_error(
                        errorCode, std::system_category(), "Socket read failed");

                return std::move(m_buffer);
            }
            catch (...)
            {
                std::throw_with_nested(Exception("Failed to read from websocket",
                    ErrorCode::networkError));
            }
        });
}

void WebSocket::close()
{
    m_httpClient.cancel();

    if (m_nested)
    {
        m_nested->cancelIOSync(aio::EventType(aio::etRead | aio::etWrite));
        m_nested->pleaseStopSync();
        m_nested.reset();
    }

    m_buffer.clear();
    m_buffer.shrink_to_fit();
}

void WebSocket::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    m_httpClient.pleaseStopSync();
    if (m_nested)
        m_nested->pleaseStopSync();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
