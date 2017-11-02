#include "ec2_connection.h"

#include <QtCore/QFileInfo>
#include <QtCore/QUrlQuery>

#include <nx/utils/std/cpp14.h>
#include "nx_ec/data/api_conversion_functions.h"
#include "transaction/transaction_message_bus.h"
#include "connection_factory.h"
#include <database/db_manager.h>

namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection(
        const Ec2DirectConnectionFactory* connectionFactory,
        ServerQueryProcessorAccess *queryProcessor,
        const QnConnectionInfo& connectionInfo,
        const nx::utils::Url& dbUrl)
    :
        BaseEc2Connection<ServerQueryProcessorAccess>( connectionFactory, queryProcessor ),
        m_connectionInfo( connectionInfo ),
        m_isInitialized( false )
    {
        // todo: #singletone. Only one connection for each connection factory allowed now
        m_isInitialized = queryProcessor->getDb()->init(dbUrl);

        connectionFactory->messageBus()->setHandler(notificationManager());

        // NOTE: Ec2StaticticsReporter can only be created after connection is established
        if (m_isInitialized)
        {
            m_staticticsReporter = std::make_unique<Ec2StaticticsReporter>(this);
        }
    }

    Ec2DirectConnection::~Ec2DirectConnection()
    {
        m_connectionFactory->messageBus()->removeHandler(notificationManager());
    }

    QnConnectionInfo Ec2DirectConnection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void Ec2DirectConnection::updateConnectionUrl(const nx::utils::Url & /*url*/)
    {
        NX_EXPECT(false, "Should never get here");
    }

    bool Ec2DirectConnection::initialized() const
    {
        return m_isInitialized;
    }

    Ec2StaticticsReporter* Ec2DirectConnection::getStaticticsReporter()
    {
        return m_staticticsReporter.get();
    }

    Timestamp Ec2DirectConnection::getTransactionLogTime() const
    {
        auto transactionLog = m_queryProcessor->getDb()->transactionLog();
        NX_ASSERT(transactionLog);
        return transactionLog ? transactionLog->getTransactionLogTime() : Timestamp();
    }

    void Ec2DirectConnection::setTransactionLogTime(Timestamp value)
    {
        NX_ASSERT(m_queryProcessor->getDb());
        auto transactionLog = m_queryProcessor->getDb()->transactionLog();
        if (transactionLog)
            transactionLog->setTransactionLogTime(value);
    }


}
