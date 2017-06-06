#pragma once

#include <QString>

namespace nx {
namespace network {

class NX_NETWORK_API AppInfo
{
public:
    static QString realm();
    static QString defaultCloudHost();
    static QString defaultCloudPortalUrl();
    static QString defaultCloudModulesXmlUrl();
    static QString cloudName();
    static QStringList compatibleCloudHosts();
};

} // namespace network
} // namespace nx
