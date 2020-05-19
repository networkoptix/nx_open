#include "camera_parameter_api.h"

#include <stdexcept>

#include <nx/utils/log/log_message.h>

#include <QtCore/QRegularExpression>
#include <QtCore/QUrlQuery>

#include "http_client.h"
#include "exception_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

namespace {

std::map<QString, QString> parse(const QString& responseBody)
{
    std::map<QString, QString> parameters;
    for (const auto line: responseBody.splitRef('\n', QString::SkipEmptyParts))
    {
        const auto equalsIndex = line.indexOf('=');
        if (equalsIndex == -1)
            throw std::runtime_error("No '=' between parameter name and value");

        const auto name = line.left(equalsIndex).trimmed();
        if (name.isEmpty())
            throw std::runtime_error("No parameter name");

        const auto quotedValue = line.right(line.length() - equalsIndex - 1).trimmed();
        if (quotedValue.isEmpty())
            throw std::runtime_error("No parameter value");
        if (quotedValue.length() < 2
            || !quotedValue.startsWith('\'') || !quotedValue.endsWith('\''))
        {
            throw std::runtime_error("Malformed parameter value");
        }
        const auto value = quotedValue.mid(1, quotedValue.length() - 2);

        parameters.emplace(name.toString(), value.toString());
    }
    return parameters;
}

} // namespace

CameraParameterApi::CameraParameterApi(Url url):
    m_url(std::move(url))
{
}

std::map<QString, QString> CameraParameterApi::fetch(
    const std::set<QString>& prefixes)
{
    try
    {
        QString query;
        for (const auto& prefix: prefixes)
            query += (query.isEmpty() ? "" : "&") + prefix;

        auto url = m_url;
        url.setPath("/cgi-bin/admin/getparam.cgi");
        url.setQuery(query);

        const auto responseBody = HttpClient().get(url).get();
        return parse(QString::fromUtf8(responseBody));
    }
    catch (const std::exception&)
    {
        rethrowWithContext("Failed to fetch parameters from camera at %1", withoutUserInfo(m_url));
    }
}

void CameraParameterApi::store(const std::map<QString, QString>& parameters)
{
    try
    {
        QUrlQuery query;
        for (const auto& [name, value]: parameters)
            query.addQueryItem(name, value);

        auto url = m_url;
        url.setScheme("http");
        url.setPath("/cgi-bin/admin/setparam.cgi");
        url.setQuery(query);

        HttpClient().get(url).get();
    }
    catch (const std::exception&)
    {
        rethrowWithContext("Failed to store parameters to camera at %1", withoutUserInfo(m_url));
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
