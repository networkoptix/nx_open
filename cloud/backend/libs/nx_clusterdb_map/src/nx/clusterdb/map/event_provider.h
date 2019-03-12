#pragma once

#include <string>
#include <vector>

#include <nx/utils/subscription.h>

namespace nx::sql { class QueryContext; }

namespace nx::clusterdb::map {

using InsertTriggerHandler = nx::utils::MoveOnlyFunc<void(
    nx::sql::QueryContext* /*queryContext*/,
    std::string /*key*/,
    std::string /*value*/)>;

using RemoveTriggerhandler = nx::utils::MoveOnlyFunc<void(
    nx::sql::QueryContext*,
    std::string /*key*/)>;

class NX_KEY_VALUE_DB_API EventProvider
{
public:
    void subscribeToRecordInserted(
        InsertTriggerHandler insertTriggerHandler,
        nx::utils::SubscriptionId* const subscriptionId);

    void subscribeToRecordRemoved(
        RemoveTriggerhandler removeTriggerHandler,
        nx::utils::SubscriptionId* const subcriptionId);

    void unsubscribeFromRecordInserted(nx::utils::SubscriptionId subscriptionid);

    void unsubscribeFromRecordRemoved(nx::utils::SubscriptionId subscriptionId);

    void notifyRecordInserted(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    void notifyRecordRemoved(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

private:
    nx::utils::Subscription<
        nx::sql::QueryContext*,
        std::string /*key*/,
        std::string /*value*/> m_recordInserted;

    nx::utils::Subscription<
        nx::sql::QueryContext*,
        std::string/*key*/> m_recordRemoved;
};

} // namespace nx::clusterdb::map
