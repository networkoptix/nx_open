/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection(
        ServerQueryProcessor* queryProcessor,
        const ResourceContext& resCtx,
        const QnConnectionInfo& connectionInfo,
        const QString& dbFileName)
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resCtx ),
        m_dbManager( new QnDbManager(resCtx.resFactory, &m_storedFileManagerImpl, dbFileName) ),
        m_connectionInfo( connectionInfo )
    {
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::startReceivingNotifications( bool /*fullSyncRequired*/)
    {
        //TODO/IMPL
    }
}
