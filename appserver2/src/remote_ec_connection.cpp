/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"


namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection(
        ClientQueryProcessor* queryProcessor,
        const ResourceContext& resCtx )
    :
        BaseEc2Connection<ClientQueryProcessor>( queryProcessor, resCtx )
    {
    }

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        //TODO/IMPL
        return QnConnectionInfo();
    }
}
