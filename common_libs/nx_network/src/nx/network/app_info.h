#pragma once

#include <QString>

namespace nx {
namespace network {

class NX_NETWORK_API AppInfo
{
public:
    static QString realm();
    static QString defaultCloudHostName();
    static QString defaultCloudPortalUrl(const QString& cloudHost);
    static QString defaultCloudModulesXmlUrl(const QString& cloudHost);
    static QString cloudName();
    static QString shortCloudName();
    static QStringList compatibleCloudHosts();
};

} // namespace network
} // namespace nx
