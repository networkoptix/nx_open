/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection( ServerQueryProcessor* queryProcessor, const ResourceContext& resCtx )
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resCtx ),
        m_dbManager( new QnDbManager(resCtx.resFactory) )
    {
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        //TODO/IMPL
        return QnConnectionInfo();
    }
}
