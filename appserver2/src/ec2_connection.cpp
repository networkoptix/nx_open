/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"

#include <QtCore/QFileInfo>
#include <QtCore/QUrlQuery>

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
        QnDbManager::instance()->init(
            resCtx.resFactory,
            dbUrl.toLocalFile(),
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
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::startReceivingNotifications() {
        QnTransactionMessageBus::instance()->start();
    }
}
