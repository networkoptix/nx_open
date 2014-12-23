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
        m_transactionLog( new QnTransactionLog(QnDbManager::instance()) ),
        m_connectionInfo( connectionInfo ),
        m_notificationReceiverID( 0 )
    {
        QnDbManager::instance()->init(
            resCtx.resFactory,
            dbUrl.toLocalFile(),
            QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path") );

        m_notificationReceiverID = QnTransactionMessageBus::instance()->addHandler( notificationManager() );
    }

    Ec2DirectConnection::~Ec2DirectConnection()
    {
        if( QnTransactionMessageBus::instance() && m_notificationReceiverID > 0 )
        {
            QnTransactionMessageBus::instance()->removeHandler( m_notificationReceiverID );
            m_notificationReceiverID = 0;
        }
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    QString Ec2DirectConnection::authInfo() const
    {
        return QString();
    }
}
