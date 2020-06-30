#pragma once

#include <cstddef>
#include <vector>

#include <nx/utils/url.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {


class CameraModuleApi
{
public:
    struct ModuleInfo
    {
        int index = -1;
        bool enabled = false;
    };

public:
    explicit CameraModuleApi(nx::utils::Url url);

    std::map<QString, ModuleInfo> fetchModuleInfos();

    void enable(const QString& name, bool enabled = true);

private:
    nx::utils::Url m_url;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
