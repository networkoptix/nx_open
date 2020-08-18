#include "camera_module_api.h"

#include <nx/utils/log/log_message.h>

#include <QtCore/QUrlQuery>

#include "camera_parameter_api.h"
#include "http_client.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

namespace {

bool parseStatus(const QString& status)
{
    if (status == "off")
        return false;
    if (status == "on")
        return true;
    throw Exception("Failed to parse module status");
}

QString getFirmwareVersion(const Url& cameraUrl)
{
    try 
    {
        CameraParameterApi api(cameraUrl);

        static const QString key = "system_info_firmwareversion";
        const auto version = at(api.fetch({key}), key);
        const auto versionParts = version.split("-");
        if (versionParts.count() < 3)
            throw Exception("Malformed firmware version: %1", version);

        return versionParts[2];
    }
    catch (nx::utils::Exception& exception)
    {
        exception.addContext("Failed to fetch firmware version");
        throw;
    }
}

} // namespace

CameraModuleApi::CameraModuleApi(Url url):
    m_url(std::move(url))
{
}

std::map<QString, CameraModuleApi::ModuleInfo> CameraModuleApi::fetchModuleInfos()
{
    try
    {
        std::map<QString, ModuleInfo> moduleInfos;

        const auto parameters = CameraParameterApi(m_url).fetch({"vadp_module"});

        if (const auto it = parameters.find("vadp_module_number"); it != parameters.end())
        {
            const auto count = parseInt(it->second);
            for (int i = 0; i < count; ++i)
            {
                auto& moduleInfo = moduleInfos[at(parameters, NX_FMT("vadp_module_i%1_name", i))];
                moduleInfo.index = i;
                moduleInfo.isEnabled = parseStatus(
                    at(parameters, NX_FMT("vadp_module_i%1_status", i)));
            }
        }

        return moduleInfos;
    }
    catch (nx::utils::Exception& exception)
    {
        exception.addContext("Failed to fetch VADP module list");
        throw;
    }
}

void CameraModuleApi::enable(const QString& name, bool isEnabled)
{
    try
    {
        const auto moduleInfo = fetchModuleInfos()[name];
        if (moduleInfo.index == -1)
            throw Exception("Not installed");
        if (moduleInfo.isEnabled == isEnabled)
            return;

        QUrlQuery query;
        query.addQueryItem("idx", QString::number(moduleInfo.index));
        query.addQueryItem("cmd", isEnabled ? "start" : "stop");

        auto url = m_url;
        url.setPath("/cgi-bin/admin/vadpctrl.cgi");
        url.setQuery(query);

        QByteArray responseBody;
        try
        {
            responseBody = HttpClient().get(url).get();
        }
        catch (const Exception& exception)
        {
            // Firmware versions before 0121 return malformed HTTP response. From here it looks as
            // if it resets the connection before the response could be parsed.
            if (exception.errorCode() != std::errc::connection_reset)
                throw;

            static const QString firstFixedVersion = "0121";
            const auto currentVersion = getFirmwareVersion(m_url);
            if (currentVersion >= firstFixedVersion)
                throw;

            throw Exception(
                "The camera is running firmware version %1, which contains some bugs, please"
                " update it to at least version %2", currentVersion, firstFixedVersion);
        }

        const auto successPattern =
            NX_FMT("'VCA is %1ed'", isEnabled ? "start" : "stopp").toUtf8();
        if (!responseBody.contains(successPattern))
        {
            throw Exception(
                "HTTP response body doesn't contain expected pattern indicating success");
        }
    }
    catch (nx::utils::Exception& exception)
    {
        exception.addContext("Failed to %1 VADP module %2",
            isEnabled ? "enable" : "disable", name);
        throw;
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
