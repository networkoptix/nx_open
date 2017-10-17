#include "system_health_info_provider.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "managers_types.h"
#include "../ec2/connection_manager.h"

namespace nx {
namespace cdb {

SystemHealthInfoProvider::SystemHealthInfoProvider(
    ec2::ConnectionManager* ec2ConnectionManager,
    nx::utils::db::AsyncSqlQueryExecutor* const dbManager)
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

    NX_LOGX(lm("Requested system %1 health history").arg(systemId.systemId), cl_logDEBUG2);

    auto resultData = std::make_unique<api::SystemHealthHistory>();
    auto resultDataPtr = resultData.get();

    m_dbManager->executeSelect(
        std::bind(&dao::rdb::SystemHealthHistoryDataObject::selectHistoryBySystem,
            &m_systemHealthHistoryDataObject, _1, systemId.systemId, resultDataPtr),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler),
            resultData = std::move(resultData)](
                nx::utils::db::QueryContext* /*queryContext*/,
                nx::utils::db::DBResult dbResult)
        {
            completionHandler(dbResultToApiResult(dbResult), std::move(*resultData));
        });
}

void SystemHealthInfoProvider::onSystemStatusChanged(
    std::string systemId, api::SystemHealth systemHealth)
{
    using namespace std::placeholders;

    NX_LOGX(lm("System %1 changed health state to %2")
        .arg(systemId).arg(QnLexical::serialized(systemHealth)), cl_logDEBUG2);

    api::SystemHealthHistoryItem healthHistoryItem;
    healthHistoryItem.state = systemHealth;
    healthHistoryItem.timestamp = nx::utils::utcTime();

    m_dbManager->executeUpdate(
        std::bind(&dao::rdb::SystemHealthHistoryDataObject::insert,
            &m_systemHealthHistoryDataObject, _1, systemId, healthHistoryItem),
        [this, systemId, locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult dbResult)
        {
            NX_LOGX(lm("Save system %1 history item finished with result %2")
                .arg(systemId).arg(dbResult), cl_logDEBUG2);
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
    ec2::ConnectionManager* ec2ConnectionManager,
    nx::utils::db::AsyncSqlQueryExecutor* const dbManager)
{
    return std::make_unique<SystemHealthInfoProvider>(
        ec2ConnectionManager,
        dbManager);
}

} // namespace cdb
} // namespace nx
