#include "camera_parameter_api.h"

#include <nx/utils/log/log_message.h>
#include <nx/vms_server_plugins/utils/exception.h>

#include <QtCore/QRegularExpression>
#include <QtCore/QUrlQuery>

#include "http_client.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;
using namespace nx::vms_server_plugins::utils;

namespace {

std::map<QString, QString> parse(const QString& responseBody)
{
    std::map<QString, QString> parameters;
    for (const auto line: responseBody.splitRef('\n', QString::SkipEmptyParts))
    {
        const auto equalsIndex = line.indexOf('=');
        if (equalsIndex == -1)
            throw Exception("No '=' between parameter name and value: %1", line);

        const auto name = line.left(equalsIndex).trimmed();
        if (name.isEmpty())
            throw Exception("No parameter name");

        const auto quotedValue = line.right(line.length() - equalsIndex - 1).trimmed();
        if (quotedValue.isEmpty())
            throw Exception("No parameter value");
        if (quotedValue.length() < 2
            || !quotedValue.startsWith('\'') || !quotedValue.endsWith('\''))
        {
            throw Exception("Malformed parameter value");
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
    catch (nx::utils::Exception& exception)
    {
        exception.addContext("Failed to fetch parameters from camera at %1",
            withoutUserInfo(m_url));
        throw;
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
    catch (nx::utils::Exception& exception)
    {
        exception.addContext("Failed to store parameters to camera at %1",
            withoutUserInfo(m_url));
        throw;
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
