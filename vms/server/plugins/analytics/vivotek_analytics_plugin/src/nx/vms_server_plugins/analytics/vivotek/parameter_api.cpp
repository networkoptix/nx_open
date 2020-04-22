#include "parameter_api.h"

#include <exception>
#include <stdexcept>

#include <QtCore/QUrlQuery>

#include "future_utils.h"
#include "http_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::network;

ParameterApi::ParameterApi(Url url):
    m_url(std::move(url))
{
    m_httpClient.emplace();

    // vivotek rejects requests with default user agent
    m_httpClient->addAdditionalHeader("User-Agent", "");
}

ParameterApi::~ParameterApi()
{
    pleaseStopSync();
}

cf::future<std::unordered_map<std::string, std::string>> ParameterApi::get(
    std::unordered_set<std::string> prefixes)
{
    return initiateFuture(
        [this, prefixes = std::move(prefixes)](auto promise)
        {
            QString query;
            for (const auto& prefix: prefixes)
                query += (query.isEmpty() ? "" : "&") + QString::fromStdString(prefix);

            auto url = m_url;
            url.setPath("/cgi-bin/admin/getparam.cgi");
            url.setQuery(query);

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
            return parseResponse();
        });
}

cf::future<std::unordered_map<std::string, std::string>> ParameterApi::set(
    std::unordered_map<std::string, std::string> parameters)
{
    return initiateFuture(
        [this, parameters = std::move(parameters)](auto promise)
        {
            QUrlQuery query;
            for (const auto& [name, value]: parameters)
                query.addQueryItem(QString::fromStdString(name), QString::fromStdString(value));

            auto url = m_url;
            url.setScheme("http");
            url.setPath("/cgi-bin/admin/setparam.cgi");
            url.setQuery(query);

            m_httpClient->doPost(url,
                [promise = std::move(promise)]() mutable
                {
                    promise.set_value(cf::unit());
                });
        }).then([this](auto future) {
            future.get();
            checkResponse(*m_httpClient);
            return parseResponse();
        });
}

void ParameterApi::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    m_httpClient.reset();
}

std::unordered_map<std::string, std::string> ParameterApi::parseResponse()
{
    auto makeError = [this]{
        return std::runtime_error(
            QString("HTTP %1 %2 response body is malformed")
                .arg(QString::fromUtf8(m_httpClient->request().requestLine.method))
                .arg(m_httpClient->url().toString())
                .toStdString());
    };

    std::unordered_map<std::string, std::string> parameters;
    for (auto entry: m_httpClient->fetchMessageBodyBuffer().split('\n'))
    {
        entry = entry.trimmed();
        if (entry.isEmpty())
            continue;

        auto keyValue = entry.split('=');
        if (keyValue.size() < 2)
            throw makeError();

        auto key = keyValue.takeFirst().toStdString();
        auto value = keyValue.join('=').toStdString();
        if (value.size() < 2)
            throw makeError();
        value = value.substr(1, value.size() - 2);

        parameters.emplace(std::move(key), std::move(value));
    }
    return parameters;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
