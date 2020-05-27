#pragma once

#include <nx/sdk/analytics/point.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>

#include <QtCore/QString>
#include <QtCore/QJsonValue>

#include "http_client.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraVcaParameterApi
{
public:
    explicit CameraVcaParameterApi(nx::utils::Url url);

    cf::future<QJsonValue> fetch(const QString& scope);
    cf::future<cf::unit> store(const QString& scope, const QJsonValue& parameters);
    cf::future<cf::unit> reloadConfig();

    static nx::sdk::analytics::Point parsePoint(const QJsonValue& json, const QString& path = "$");
    static QJsonObject unparse(const nx::sdk::analytics::Point& point);

private:
    nx::utils::Url m_url;
    HttpClient m_httpClient;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
