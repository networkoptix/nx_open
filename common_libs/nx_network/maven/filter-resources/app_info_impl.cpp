//
// This file is generated. Go to pom.xml.
//
#include "nx/network/app_info.h"
#include "nx/network/cloud/cloud_connect_controller.h"
#include "nx/network/socket_global.h"

namespace nx {
namespace network {

QString AppInfo::realm()
{
    return QStringLiteral("VMS");
}

//Filling string constant with zeros to be able to change this constant in already-built binary
static const char* kCloudHostNameWithPrefix = "this_is_cloud_host_name ${cloudHost}\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
static const char* kCloudHostName = kCloudHostNameWithPrefix + sizeof("this_is_cloud_host_name");

QString AppInfo::defaultCloudHost()
{
#ifdef _DEBUG
    const QString overriddenHost = SocketGlobals::cloud().ini().cloudHost;
    if (!overriddenHost.isEmpty())
        return overriddenHost;
#endif

    return QString::fromUtf8(kCloudHostName);
}

QString AppInfo::defaultCloudPortalUrl()
{
    return QString::fromLatin1("https://%1").arg(defaultCloudHost());
}

QString AppInfo::defaultCloudModulesXmlUrl()
{
    return QString::fromLatin1("http://%1/api/cloud_modules.xml").arg(defaultCloudHost());
}

QString AppInfo::cloudName()
{
    return QStringLiteral("${cloudName}");
}

QStringList AppInfo::compatibleCloudHosts()
{
    const auto hostsString = QString::fromLatin1("${compatibleCloudHosts}");
    return hostsString.split(L';');
}

} // namespace network
} // namespace nx
