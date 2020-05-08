#pragma once

#include <map>
#include <set>

#include <nx/utils/url.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraParameterApi
{
public:
    explicit CameraParameterApi(nx::utils::Url url);

    std::map<QString, QString> fetch(const std::set<QString>& prefixes);
    void store(const std::map<QString, QString>& parameters);

private:
    nx::utils::Url m_url;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
