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

QString AppInfo::defaultCloudHostName()
{
    return QString::fromUtf8(kCloudHostName);
}

QString AppInfo::defaultCloudPortalUrl(const QString& cloudHost)
{
    return QString::fromLatin1("https://%1").arg(cloudHost);
}

QString AppInfo::defaultCloudModulesXmlUrl(const QString& cloudHost)
{
    return QString::fromLatin1("http://%1/discovery/v1/cloud_modules.xml").arg(cloudHost);
}

QString AppInfo::cloudName()
{
    return QStringLiteral("${cloudName}");
}

QString AppInfo::shortCloudName()
{
    return QStringLiteral("${shortCloudName}");
}

QStringList AppInfo::compatibleCloudHosts()
{
    const auto hostsString = QString::fromLatin1("${compatibleCloudHosts}");
    return hostsString.split(L';');
}

} // namespace network
} // namespace nx
