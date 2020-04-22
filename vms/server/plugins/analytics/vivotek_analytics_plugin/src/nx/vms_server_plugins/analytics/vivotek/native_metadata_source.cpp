#include "native_metadata_source.h"

#include <stdexcept>
#include <system_error>

#include <nx/network/aio/event_type.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/utils/thread/cf/cfuture.h>

#include "future_utils.h"
#include "http_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::utils;
using namespace nx::network;

NativeMetadataSource::NativeMetadataSource()
{
}

NativeMetadataSource::~NativeMetadataSource()
{
    pleaseStopSync();
}

cf::future<cf::unit> NativeMetadataSource::open(const Url& url)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, url = url](auto future) mutable
        {
            future.get();

            m_httpClient.emplace();

            return ensureDetailMetadataMode(std::move(url));
        })
    .then(
        [this, url = url](auto future) mutable
        {
            future.get();

            url.setScheme("ws:");
            url.setPath("/ws/vca");
            url.setQuery("?data=meta,event");

            websocket::addClientHeaders(&*m_httpClient,
                "tracker-protocol", websocket::CompressionType::perMessageDeflate);

            return initiateFuture(
                [this, url = std::move(url)](auto promise) mutable
                {
                    m_httpClient->doGet(url,
                        [promise = std::move(promise)]() mutable
                        {
                            promise.set_value(cf::unit());
                        });
                });
        })
    .then(
        [this](auto future)
        {
            future.get();

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

            return cf::unit();
        });
}

cf::future<Json> NativeMetadataSource::read()
{
    return initiateFuture<SystemError::ErrorCode>(
        [this](auto promise) mutable
        {
            m_buffer.clear();
            m_websocket->readSomeAsync(&m_buffer,
                [promise = std::move(promise)](auto errorCode, auto /*size*/) mutable
                {
                    promise.set_value(errorCode);
                });
        })
    .then(
        [this](auto future)
        {
            if (auto errorCode = future.get())
                throw std::system_error(
                    errorCode, std::system_category(), "HTTP connection failed");

            std::string error;
            auto metadata = Json::parse(m_buffer.toStdString(), error);
            if (!error.empty())
                throw std::runtime_error("Failed to parse json: " + error);

            return metadata;
        });
}

void NativeMetadataSource::close()
{
    m_httpClient.reset();

    if (m_websocket)
    {
        m_websocket->pleaseStopSync();
        m_websocket.reset();
    }
}

void NativeMetadataSource::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();

    m_httpClient.reset();

    if (m_websocket)
    {
        m_websocket->pleaseStopSync();
        m_websocket.reset();
    }
}

cf::future<cf::unit> NativeMetadataSource::ensureDetailMetadataMode(Url url)
{
    // 'Detail metadata mode' (undocumented as of 2020.04.16) enables, among others,
    // a `Pos2D` field describing object boundaries in normalized screen space.
    return initiateFuture<Url>(
        [this, url = std::move(url)](auto promise) mutable
        {
            url.setPath("/VCA/Config/AE/WebSocket/DetailMetadata");
            m_httpClient->doGet(url,
                [promise = std::move(promise), url]() mutable
                {
                    promise.set_value(std::move(url));
                });
        })
    .then(
        [this](auto future) mutable
        {
            auto url = future.get();

            checkResponse(*m_httpClient);

            const auto responseBody = m_httpClient->fetchMessageBodyBuffer();
            if (responseBody.trimmed() == "true") // already enabled
                return cf::make_ready_future(cf::unit());

            return setDetailMetadataMode(std::move(url));
        });
}

cf::future<cf::unit> NativeMetadataSource::setDetailMetadataMode(Url url)
{
    return initiateFuture(
        [this, url](auto promise) mutable
        {
            m_httpClient->setRequestBody(
                std::make_unique<http::BufferSource>("application/json", "true"));
            m_httpClient->doPost(url,
                [promise = std::move(promise)]() mutable
                {
                    promise.set_value(cf::unit());
                });
        })
    .then(
        [this, url = url](auto future) mutable
        {
            future.get();

            checkResponse(*m_httpClient);

            return reloadConfig(std::move(url));
        });
}

cf::future<cf::unit> NativeMetadataSource::reloadConfig(Url url)
{
    return initiateFuture(
        [this, url = std::move(url)](auto promise) mutable
        {
            url.setPath("/VCA/Config/Reload");

            m_httpClient->doGet(url,
                [promise = std::move(promise)]() mutable
                {
                    promise.set_value(cf::unit());
                });
        })
    .then(
        [this](auto future)
        {
            future.get();

            checkResponse(*m_httpClient);

            return cf::unit();
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
