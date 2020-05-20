#pragma once

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>

#include <QtCore/QString>
#include <QtCore/QJsonValue>

#include "http_client.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraVcaParameterApi
{
public:
    explicit CameraVcaParameterApi(nx::utils::Url url, const QString& scope);

    cf::future<QJsonValue> fetch();
    cf::future<cf::unit> store(const QJsonValue& parameters);

private:
    nx::utils::Url m_url;
    HttpClient m_httpClient;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
