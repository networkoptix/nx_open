#include "native_metadata_source.h"

#include <stdexcept>
#include <system_error>

#include <nx/network/aio/event_type.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/websocket/websocket_handshake.h>

#include "http_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::utils;
using namespace nx::network;

NativeMetadataSource::NativeMetadataSource()
{
    m_httpClient.emplace();
}

NativeMetadataSource::~NativeMetadataSource()
{
    pleaseStopSync();
}

void NativeMetadataSource::open(const Url& baseUrl, MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    ensureDetailMetadataMode(baseUrl,
        [this, baseUrl, handler = std::move(handler)](auto exceptionPtr) mutable
        {
            if (exceptionPtr)
            {
                handler(exceptionPtr);
                return;
            }

            Url url = baseUrl;
            url.setScheme("ws:");
            url.setPath("/ws/vca");
            url.setQuery("?data=meta,event");

            websocket::addClientHeaders(&*m_httpClient,
                "tracker-protocol", websocket::CompressionType::perMessageDeflate);

            m_httpClient->doGet(url,
                [this, handler = std::move(handler)]() mutable
                {
                    try
                    {
                        if (m_httpClient->failed())
                            throw std::system_error(
                                m_httpClient->lastSysErrorCode(), std::system_category(),
                                "HTTP transport connection failed: " + m_httpClient->url().toStdString());

                        const auto error = websocket::validateResponse(
                            m_httpClient->request(), *m_httpClient->response());
                        if (error != websocket::Error::noError)
                            throw std::runtime_error("Websocket handshake failed");

                        m_websocket.emplace(
                            m_httpClient->takeSocket(),
                            websocket::Role::client,
                            websocket::FrameType::text,
                            websocket::compressionType(m_httpClient->response()->headers));

                        m_websocket->start();

                        handler(nullptr);
                    }
                    catch (...)
                    {
                        handler(std::current_exception());
                    }
                });
        });
}

void NativeMetadataSource::read(MoveOnlyFunc<void(std::exception_ptr, Json)> handler)
{
    m_websocket->readSomeAsync(&m_buffer,
        [this, handler = std::move(handler)](auto errorCode, auto /*size*/) mutable
        {
            try
            {
                if (errorCode)
                    throw std::system_error(
                        errorCode, std::system_category(), "HTTP connection failed");

                std::string error;
                auto metadata = Json::parse(m_buffer.toStdString(), error);
                if (!error.empty())
                    throw std::runtime_error("Failed to parse json: " + error);

                handler(nullptr, std::move(metadata));
            }
            catch (...)
            {
                handler(std::current_exception(), Json());
            }
        });
}

void NativeMetadataSource::close(nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_httpClient.emplace();

            if (!m_websocket)
            {
                handler();
                return;
            }

            m_websocket->cancelIOAsync(aio::EventType::etRead,
                [this, handler = std::move(handler)]() mutable {
                    m_websocket->sendCloseAsync();
                    m_websocket.reset();

                    handler();
                });
        });
}

void NativeMetadataSource::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    if (m_httpClient)
        m_httpClient.reset();
    if (m_websocket)
        m_websocket->pleaseStopSync();
}

void NativeMetadataSource::ensureDetailMetadataMode(Url url,
    MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    // 'Detail metadata mode' (undocumented as of 2020.04.16) enables, among others,
    // a `Pos2D` field describing object boundaries in normalized screen space.

    url.setScheme("http:");
    url.setPath("/VCA/Config/AE/WebSocket/DetailMetadata");
    m_httpClient->doGet(url,
        [this, url, handler = std::move(handler)]() mutable
        {
            try
            {
                checkResponse(*m_httpClient);

                if (m_httpClient->fetchMessageBodyBuffer().trimmed() == "true")
                {
                    handler(nullptr);
                    return;
                }

                setDetailMetadataMode(url, std::move(handler));
            }
            catch (...)
            {
                handler(std::current_exception());
            }
        });
}

void NativeMetadataSource::setDetailMetadataMode(Url url,
    MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    m_httpClient->setRequestBody(
        std::make_unique<http::BufferSource>("application/json", "true"));
    m_httpClient->doPost(url,
        [this, url, handler = std::move(handler)]() mutable
        {
            try
            {
                checkResponse(*m_httpClient);

                reloadConfig(url, std::move(handler));
            }
            catch (...)
            {
                handler(std::current_exception());
            }
        });
}

void NativeMetadataSource::reloadConfig(Url url,
    MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    url.setPath("/VCA/Config/Reload");
    m_httpClient->doGet(url,
        [this, url, handler = std::move(handler)]() mutable
        {
            try
            {
                checkResponse(*m_httpClient);

                handler(nullptr);
            }
            catch (...)
            {
                handler(std::current_exception());
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
