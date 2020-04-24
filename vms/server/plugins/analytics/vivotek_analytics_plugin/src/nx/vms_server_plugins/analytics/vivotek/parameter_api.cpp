#include "parameter_api.h"

#include <algorithm>
#include <stdexcept>

#include <QtCore/QUrlQuery>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::network;

namespace
{
    std::unordered_map<std::string, std::string> parseResponse(const http::Response& response)
    {
        const auto statusCode = response.statusLine.statusCode;
        if (response.statusLine.statusCode != 200)
            throw std::runtime_error(
                "Camera returned unexpected HTTP status code " + std::to_string(statusCode));

        std::unordered_map<std::string, std::string> parameters;
        for (auto entry: response.messageBody.split('\n'))
        {
            entry = entry.trimmed();
            if (entry.isEmpty())
                continue;

            auto equals = std::find(entry.begin(), entry.end(), '=');
            if (equals == entry.end())
                throw std::runtime_error("Camera returned malformed parameters");

            std::string key(entry.begin(), equals);
            const std::string quotedValue(equals + 1, entry.end());
            if (quotedValue.size() < 2)
                throw std::runtime_error("Camera returned malformed parameters");
            std::string value(quotedValue.begin() + 1, quotedValue.end() - 1);

            parameters.emplace(std::move(key), std::move(value));
        }

        return parameters;
    }
}

ParameterApi::ParameterApi(Url url):
    m_url(std::move(url))
{
}

ParameterApi::~ParameterApi()
{
}

void ParameterApi::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_httpClient.bindToAioThread(aioThread);
}

cf::future<std::unordered_map<std::string, std::string>> ParameterApi::get(
    std::unordered_set<std::string> prefixes)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, prefixes = std::move(prefixes)](auto future)
        {
            future.get();

            QString query;
            for (const auto& prefix: prefixes)
                query += (query.isEmpty() ? "" : "&") + QString::fromStdString(prefix);

            auto url = m_url;
            url.setPath("/cgi-bin/admin/getparam.cgi");
            url.setQuery(query);

            // vivotek rejects requests with default user agent
            m_httpClient.addAdditionalRequestHeader("User-Agent", "");

            return m_httpClient.get(std::move(url));
        })
    .then(
        [](auto future)
        {
            try
            {
                return parseResponse(future.get());
            }
            catch (...)
            {
                std::throw_with_nested(std::runtime_error("Failed to get camera parameters"));
            }
        });
}

cf::future<std::unordered_map<std::string, std::string>> ParameterApi::set(
    std::unordered_map<std::string, std::string> parameters)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, parameters = std::move(parameters)](auto future)
        {
            future.get();

            QUrlQuery query;
            for (const auto& [name, value]: parameters)
                query.addQueryItem(QString::fromStdString(name), QString::fromStdString(value));

            auto url = m_url;
            url.setScheme("http");
            url.setPath("/cgi-bin/admin/setparam.cgi");
            url.setQuery(query);

            // vivotek rejects requests with default user agent
            m_httpClient.addAdditionalRequestHeader("User-Agent", "");

            return m_httpClient.get(std::move(url));
        })
    .then(
        [](auto future)
        {
            try
            {
                return parseResponse(future.get());
            }
            catch (...)
            {
                std::throw_with_nested(std::runtime_error("Failed to set camera parameters"));
            }
        });
}

void ParameterApi::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    m_httpClient.pleaseStopSync();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
