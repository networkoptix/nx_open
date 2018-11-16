#include "system_health_info_provider.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "managers_types.h"

namespace nx::cloud::db {

SystemHealthInfoProvider::SystemHealthInfoProvider(
    clusterdb::engine::ConnectionManager* ec2ConnectionManager,
    nx::sql::AsyncSqlQueryExecutor* const dbManager)
    :
    m_ec2ConnectionManager(ec2ConnectionManager),
    m_dbManager(dbManager),
    m_systemStatusChangedSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
    using namespace std::placeholders;

    m_ec2ConnectionManager->systemStatusChangedSubscription().subscribe(
        std::bind(&SystemHealthInfoProvider::onSystemStatusChanged, this, _1, _2),
        &m_systemStatusChangedSubscriptionId);
}

SystemHealthInfoProvider::~SystemHealthInfoProvider()
{
    m_ec2ConnectionManager->systemStatusChangedSubscription().removeSubscription(
        m_systemStatusChangedSubscriptionId);

    m_startedAsyncCallsCounter.wait();
}

bool SystemHealthInfoProvider::isSystemOnline(
    const std::string& systemId) const
{
    return m_ec2ConnectionManager->isSystemConnected(systemId);
}

void SystemHealthInfoProvider::getSystemHealthHistory(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId systemId,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("Requested system %1 health history").arg(systemId.systemId));

    auto resultData = std::make_unique<api::SystemHealthHistory>();
    auto resultDataPtr = resultData.get();

    m_dbManager->executeSelect(
        std::bind(&dao::rdb::SystemHealthHistoryDataObject::selectHistoryBySystem,
            &m_systemHealthHistoryDataObject, _1, systemId.systemId, resultDataPtr),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler),
            resultData = std::move(resultData)](
                nx::sql::DBResult dbResult)
        {
            completionHandler(dbResultToApiResult(dbResult), std::move(*resultData));
        });
}

void SystemHealthInfoProvider::onSystemStatusChanged(
    const std::string& systemId,
    clusterdb::engine::SystemStatusDescriptor statusDescription)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, lm("System %1 changed health state to %2")
        .args(systemId, statusDescription.isOnline ? "online" : "offline"));

    api::SystemHealthHistoryItem healthHistoryItem;
    healthHistoryItem.state = statusDescription.isOnline ? api::SystemHealth::online : api::SystemHealth::offline;
    healthHistoryItem.timestamp = nx::utils::utcTime();

    m_dbManager->executeUpdate(
        std::bind(&dao::rdb::SystemHealthHistoryDataObject::insert,
            &m_systemHealthHistoryDataObject, _1, systemId, healthHistoryItem),
        [this, systemId, locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::sql::DBResult dbResult)
        {
            NX_VERBOSE(this, lm("Save system %1 history item finished with result %2")
                .arg(systemId).arg(dbResult));
        });
}

//-------------------------------------------------------------------------------------------------

using namespace std::placeholders;

SystemHealthInfoProviderFactory::SystemHealthInfoProviderFactory():
    base_type(std::bind(&SystemHealthInfoProviderFactory::defaultFactory, this, _1, _2))
{
}

SystemHealthInfoProviderFactory& SystemHealthInfoProviderFactory::instance()
{
    static SystemHealthInfoProviderFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractSystemHealthInfoProvider> SystemHealthInfoProviderFactory::defaultFactory(
    clusterdb::engine::ConnectionManager* ec2ConnectionManager,
    nx::sql::AsyncSqlQueryExecutor* const dbManager)
{
    return std::make_unique<SystemHealthInfoProvider>(
        ec2ConnectionManager,
        dbManager);
}

} // namespace nx::cloud::db
