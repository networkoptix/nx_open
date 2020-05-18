#include "websocket.h"

#include <exception>
#include <stdexcept>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_message.h>
#include <nx/network/websocket/websocket_handshake.h>

#include "exception_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::network;

cf::future<cf::unit> WebSocket::open(const Url& url)
{
    close();

    m_httpClient.emplace();

    http::HttpHeaders headers;
    websocket::addClientHeaders(&headers,
        "tracker-protocol", websocket::CompressionType::perMessageDeflate);
    for (const auto& [name, value]: headers)
        m_httpClient->addAdditionalHeader(name, value);

    return m_httpClient->get(url)
        .then_unwrap(
            [this](auto&&)
            {
                const auto& request = m_httpClient->request();
                const auto& response = *m_httpClient->response();

                const auto error = websocket::validateResponse(request, response);
                if (error != websocket::Error::noError)
                    throw std::runtime_error("Handshake failed");

                m_nested.emplace(
                    m_httpClient->takeSocket(),
                    websocket::Role::client,
                    websocket::FrameType::text,
                    websocket::compressionType(response.headers));

                m_httpClient.reset();

                m_nested->start();

                return cf::unit();
            })
        .then(addExceptionContext(
            "Failed to connect to websocket server at %1", withoutUserInfo(std::move(url))));
}

cf::future<nx::Buffer> WebSocket::read()
{
    return cf::initiate(
        [this]
        {
            if (!NX_ASSERT(m_nested))
                throw std::logic_error("Not connected");

            m_buffer.clear();
            return m_nested->readSome(&m_buffer);
        })
        .then_unwrap([this](auto&&) { return std::move(m_buffer); })
        .then(addExceptionContext("Failed to read from websocket"));
}

void WebSocket::close()
{
    m_buffer.clear();
    m_buffer.shrink_to_fit();

    m_httpClient.reset();
    m_nested.reset();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
