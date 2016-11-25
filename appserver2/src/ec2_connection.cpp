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
    Ec2DirectConnection::Ec2DirectConnection(ServerQueryProcessorAccess *queryProcessor,
        const QnConnectionInfo& connectionInfo,
        const QUrl& dbUrl)
    :
        BaseEc2Connection<ServerQueryProcessorAccess>( queryProcessor ),
        m_transactionLog( new QnTransactionLog(detail::QnDbManager::instance()) ),
        m_connectionInfo( connectionInfo ),
        m_isInitialized( false )
    {
        m_isInitialized = detail::QnDbManager::instance()->init(dbUrl);

        QnTransactionMessageBus::instance()->setHandler( notificationManager() );

        // NOTE: Ec2StaticticsReporter can only be created after connection is established
        if (m_isInitialized)
        {
            m_staticticsReporter = std::make_unique<Ec2StaticticsReporter>(
                getMediaServerManager(Qn::kSystemAccess));
        }
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
