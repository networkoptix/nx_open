#include "native_metadata_source.h"

#include <exception>
#include <stdexcept>
#include <system_error>

#include <nx/network/http/buffer_source.h>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::utils;
using namespace nx::network;

namespace {
    const char kDetailMetadataModePath[] = "/VCA/Config/AE/WebSocket/DetailMetadata";
}

NativeMetadataSource::NativeMetadataSource()
{
}

NativeMetadataSource::~NativeMetadataSource()
{
    pleaseStopSync();
}

cf::future<cf::unit> NativeMetadataSource::open(const Url& url)
{
    return setDetailMetadataMode(url, true).then(
        [this, url = url](auto future) mutable
        {
            future.get();

            url.setScheme("ws:");
            url.setPath("/ws/vca");
            url.setQuery("?data=meta,event");

            return m_websocket.connect(std::move(url));
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
                    "Failed to open native metadata source"));
            }
        });
}

cf::future<Json> NativeMetadataSource::read()
{
    return m_websocket.read().then(
        [](auto future)
        {
            const auto buffer = future.get();

            std::string error;
            auto metadata = Json::parse(buffer.toStdString(), error);
            if (!error.empty())
                throw std::runtime_error("Received malformed metadata json: " + error);

            return metadata;
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
                    "Failed to read native metadata"));
            }
        });
}

void NativeMetadataSource::close()
{
    m_httpClient.cancel();
    m_websocket.close();
}

void NativeMetadataSource::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    m_httpClient.pleaseStopSync();
    m_websocket.pleaseStopSync();
}

cf::future<bool> NativeMetadataSource::getDetailMetadataMode(Url url)
{
    // 'Detail metadata mode' (undocumented as of 2020.04.16) enables, among others,
    // a `Pos2D` field describing object boundaries in normalized screen space.
    return cf::make_ready_future(cf::unit()).then(
        [this, url = std::move(url)](auto future) mutable
        {
            future.get();

            url.setPath(kDetailMetadataModePath);

            return m_httpClient.get(std::move(url));
        })
    .then(
        [](auto future) mutable
        {
            const auto response = future.get();

            const auto statusCode = response.statusLine.statusCode;
            if (response.statusLine.statusCode != 200)
                throw std::runtime_error(
                    "Camera returned unexpected HTTP status code " + std::to_string(statusCode));

            return response.messageBody.trimmed() != "false";
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
                    "Failed to get detail metadata mode"));
            }
        });
}

cf::future<cf::unit> NativeMetadataSource::setDetailMetadataMode(const Url& url, bool value)
{
    return getDetailMetadataMode(url).then(
        [this, url = url, value](auto future) mutable
        {
            const auto oldValue = future.get();
            if (oldValue == value)
                return cf::make_ready_future(cf::unit());

            url.setPath(kDetailMetadataModePath);

            m_httpClient.setRequestBody(std::make_unique<http::BufferSource>(
                "application/json", value ? "true" : "false"));

            return m_httpClient.post(std::move(url)).then(
                [this, url](auto future) mutable
                {
                    const auto response = future.get();

                    const auto statusCode = response.statusLine.statusCode;
                    if (response.statusLine.statusCode != 200)
                        throw std::runtime_error(
                            "Camera returned unexpected HTTP status code "
                            + std::to_string(statusCode));

                    return reloadConfig(std::move(url));
                });
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
                    "Failed to set detail metadata mode"));
            }
        });
}

cf::future<cf::unit> NativeMetadataSource::reloadConfig(Url url)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, url = std::move(url)](auto future) mutable
        {
            future.get();

            url.setPath("/VCA/Config/Reload");

            return m_httpClient.get(std::move(url));
        })
    .then(
        [](auto future) mutable
        {
            const auto response = future.get();

            const auto statusCode = response.statusLine.statusCode;
            if (response.statusLine.statusCode != 200)
                throw std::runtime_error(
                    "Camera returned unexpected HTTP status code " + std::to_string(statusCode));

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
                    "Failed to get detail metadata mode"));
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
