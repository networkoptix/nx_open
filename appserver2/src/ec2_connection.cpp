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
        const QString& dbFilePath)
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resCtx ),
        m_dbManager( new QnDbManager(
            resCtx.resFactory,
            &m_licenseManagerImpl,
            dbFilePath) ),
        m_auxManager(new QnAuxManager(&m_emailManagerImpl)),
        m_transactionLog( new QnTransactionLog(m_dbManager.get() )),
        m_connectionInfo( connectionInfo )
    {
        m_dbManager->init();
        m_transactionLog->init();
        QnTransactionMessageBus::instance()->setHandler(this);
    }

    Ec2DirectConnection::~Ec2DirectConnection()
    {
        QnTransactionMessageBus::instance()->removeHandler(this);
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::startReceivingNotifications( bool /*isClient*/)
    {
        //TODO/IMPL
    }
}
