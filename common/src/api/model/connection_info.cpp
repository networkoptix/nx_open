/**********************************************************
* 6 nov 2014
* a.kolesnikov
***********************************************************/

#include "connection_info.h"

#include <nx_ec/ec_proto_version.h>


QnConnectionInfo::QnConnectionInfo()
:
    allowSslConnections( false ),
    nxClusterProtoVersion( nx_ec::INITIAL_EC2_PROTO_VERSION )
{
}
