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
        m_auxManager( new QnAuxManager() ),
        m_transactionLog( new QnTransactionLog(QnDbManager::instance()) ),
        m_connectionInfo( connectionInfo )
    {
        ec2::QnDistributedMutexManager::initStaticInstance( new ec2::QnDistributedMutexManager() );

        QnDbManager::instance()->init(
            resCtx.resFactory,
            dbUrl.path(),
            QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path") );

        ApiResourceParamDataList paramList;
        QnDbManager::instance()->doQueryNoLock(nullptr, paramList);

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
