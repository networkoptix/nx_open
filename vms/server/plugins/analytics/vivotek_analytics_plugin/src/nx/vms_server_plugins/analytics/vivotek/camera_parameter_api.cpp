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
    static const QRegularExpression nameValueRe(
        R"REGEX(^\s*(?P<name>[^=]+)\s*=\s*'(?P<value>[^']*)'\s*$)REGEX",
        QRegularExpression::MultilineOption);

    static const std::runtime_error parseError("Failed to parse fetched parameters");

    std::map<QString, QString> parameters;
    for (auto matchIter = nameValueRe.globalMatch(responseBody); matchIter.hasNext(); )
    {
        auto match = matchIter.next();
        parameters.emplace(match.captured("name"), match.captured("value"));
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
