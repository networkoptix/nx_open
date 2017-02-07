#include "ec2_connection.h"

#include <QtCore/QFileInfo>
#include <QtCore/QUrlQuery>

#include <nx/utils/std/cpp14.h>
#include "nx_ec/data/api_conversion_functions.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection(ServerQueryProcessorAccess *queryProcessor,
        const QnConnectionInfo& connectionInfo,
        const QUrl& dbUrl)
    :
        BaseEc2Connection<ServerQueryProcessorAccess>( queryProcessor ),
        m_connectionInfo( connectionInfo ),
        m_isInitialized( false )
    {
        // todo: #singletone. Only one connection for each connection factory allowed now
        m_isInitialized = queryProcessor->getDb()->init(dbUrl);

        QnTransactionMessageBus::instance()->setHandler( notificationManager() );

        // NOTE: Ec2StaticticsReporter can only be created after connection is established
        if (m_isInitialized)
        {
            m_staticticsReporter = std::make_unique<Ec2StaticticsReporter>(this);
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
