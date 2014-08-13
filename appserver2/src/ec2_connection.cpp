/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"

#include <QtCore/QUrlQuery>

#include "mutex/distributed_mutex.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection(
        ServerQueryProcessor* queryProcessor,
        const ResourceContext& resCtx,
        const QnConnectionInfo& connectionInfo,
        const QUrl& dbUrl)
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resCtx ),
        m_dbManager( new QnDbManager(
            resCtx.resFactory,
            &m_licenseManagerImpl,
            dbUrl.path().mid(1),
            QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path"))),
        m_auxManager(new QnAuxManager()),
        m_transactionLog( new QnTransactionLog(m_dbManager.get() )),
        m_connectionInfo( connectionInfo )
    {
        ec2::QnDistributedMutexManager::initStaticInstance( new ec2::QnDistributedMutexManager() );

        m_dbManager->init();

        ApiResourceParamDataList paramList;
        m_dbManager->doQueryNoLock(nullptr, paramList);

        QnTransactionMessageBus::instance()->setHandler( notificationManager() );
        QnTransactionMessageBus::instance()->setLocalPeer(ApiPeerData(qnCommon->moduleGUID(), Qn::PT_Server));
    }

    Ec2DirectConnection::~Ec2DirectConnection()
    {
        if (QnTransactionMessageBus::instance())
            QnTransactionMessageBus::instance()->removeHandler( notificationManager() );
        ec2::QnDistributedMutexManager::initStaticInstance(0);
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::startReceivingNotifications() {
        QnTransactionMessageBus::instance()->start();
    }
}
