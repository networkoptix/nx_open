/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"
#include "mutex/distributed_mutex.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "transaction/transaction_message_bus.h"


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
        ec2::QnDistributedMutexManager::initStaticInstance( new ec2::QnDistributedMutexManager() );

        m_dbManager->init();

        ApiResourceParamDataList paramList;
        m_dbManager->doQueryNoLock(nullptr, paramList);

        QnKvPairList kvPairs;
        fromApiToResourceList(paramList, kvPairs);

        m_emailManagerImpl.configure(kvPairs);
        QnTransactionMessageBus::instance()->setHandler(this);
    }

    Ec2DirectConnection::~Ec2DirectConnection()
    {
        if (QnTransactionMessageBus::instance())
            QnTransactionMessageBus::instance()->removeHandler(this);
        ec2::QnDistributedMutexManager::initStaticInstance(0);
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::startReceivingNotifications( bool /*isClient*/)
    {
        QnTransactionMessageBus::instance()->start();
    }
}
