#include "ec2_connection.h"

#include <QtCore/QFileInfo>
#include <QtCore/QUrlQuery>

#include <nx/utils/std/cpp14.h>
#include "nx_ec/data/api_conversion_functions.h"
#include "transaction/transaction_message_bus.h"
#include <local_connection_factory.h>

namespace ec2 {

Ec2DirectConnection::Ec2DirectConnection(
    const LocalConnectionFactory* connectionFactory,
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

    // NOTE: Ec2StaticticsReporter can only be created after connection is established
    if (m_isInitialized)
    {
        m_staticticsReporter = std::make_unique<Ec2StaticticsReporter>(this);
    }

    m_orphanCameraWatcher = std::make_unique<nx::appserver::OrphanCameraWatcher>(commonModule());
}

Ec2DirectConnection::~Ec2DirectConnection()
{
}

ec2::detail::QnDbManager* Ec2DirectConnection::getDb() const
{
    return m_queryProcessor->getDb();
}

QnConnectionInfo Ec2DirectConnection::connectionInfo() const
{
    return m_connectionInfo;
}

void Ec2DirectConnection::updateConnectionUrl(const nx::utils::Url & /*url*/)
{
    NX_ASSERT(false, "Should never get here");
}

bool Ec2DirectConnection::initialized() const
{
    return m_isInitialized;
}

Ec2StaticticsReporter* Ec2DirectConnection::getStaticticsReporter()
{
    return m_staticticsReporter.get();
}

nx::appserver::OrphanCameraWatcher* Ec2DirectConnection::orphanCameraWatcher()
{
    return m_orphanCameraWatcher.get();
}

nx::vms::api::Timestamp Ec2DirectConnection::getTransactionLogTime() const
{
    auto transactionLog = m_queryProcessor->getDb()->transactionLog();
    NX_ASSERT(transactionLog);
    return transactionLog ? transactionLog->getTransactionLogTime() : nx::vms::api::Timestamp();
}

void Ec2DirectConnection::setTransactionLogTime(nx::vms::api::Timestamp value)
{
    NX_ASSERT(m_queryProcessor->getDb());
    auto transactionLog = m_queryProcessor->getDb()->transactionLog();
    if (transactionLog)
        transactionLog->setTransactionLogTime(value);
}

void Ec2DirectConnection::startReceivingNotifications()
{
    base_type::startReceivingNotifications();
    m_orphanCameraWatcher->doStart();
}
} // namespace ec2
