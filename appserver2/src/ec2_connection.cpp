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
        m_isInitialized( false )
    {
        m_isInitialized = QnDbManager::instance()->init(
            resCtx.resFactory,
            dbUrl );

        QnTransactionMessageBus::instance()->setHandler( notificationManager() );

        // NOTE: Ec2StaticticsReporter can only be created after connection is established
        if (m_isInitialized)
            m_staticticsReporter.reset(new Ec2StaticticsReporter(
                getResourceManager(), getMediaServerManager()));
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

    QString Ec2DirectConnection::authInfo() const
    {
        return QString();
    }

    bool Ec2DirectConnection::initialized() const
    {
        return m_isInitialized;
    }

    Ec2StaticticsReporter* Ec2DirectConnection::getStaticticsReporter()
    {
        return m_staticticsReporter.get();
    }
}
