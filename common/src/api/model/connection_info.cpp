/**********************************************************
* 6 nov 2014
* a.kolesnikov
***********************************************************/

#include "connection_info.h"

#include <nx/network/app_info.h>
#include <nx/network/socket_global.h>

#include <nx_ec/ec_proto_version.h>
#include <utils/common/app_info.h>

QnConnectionInfo::QnConnectionInfo():
    allowSslConnections(false),
    nxClusterProtoVersion(nx_ec::INITIAL_EC2_PROTO_VERSION),
    ecDbReadOnly(false),
    newSystem(false),
    cloudHost(nx::network::SocketGlobals::cloudHost()),
    p2pMode(false)
{
}

QnUuid QnConnectionInfo::serverId() const
{
    return QnUuid::fromStringSafe(ecsGuid);
}

QUrl QnConnectionInfo::effectiveUrl() const
{
    if (!allowSslConnections)
        return ecUrl;

    QUrl secure(ecUrl);
    secure.setScheme(lit("https"));
    return secure;
}
