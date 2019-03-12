#include "event_provider.h"

#include <nx/sql/query_context.h>

namespace nx::clusterdb::map {

void EventProvider::subscribeToRecordInserted(
    InsertTriggerHandler insertTriggerHandler,
    nx::utils::SubscriptionId* const subscriptionId)
{
    m_recordInserted.subscribe(std::move(insertTriggerHandler), subscriptionId);
}

void EventProvider::subscribeToRecordRemoved(
    RemoveTriggerhandler removeTriggerHandler,
    nx::utils::SubscriptionId * const subscriptionId)
{
    m_recordRemoved.subscribe(std::move(removeTriggerHandler), subscriptionId);
}

void EventProvider::unsubscribeFromRecordInserted(nx::utils::SubscriptionId subscriptionId)
{
    m_recordInserted.removeSubscription(subscriptionId);
}

void EventProvider::unsubscribeFromRecordRemoved(nx::utils::SubscriptionId subscriptionId)
{
    m_recordRemoved.removeSubscription(subscriptionId);
}

void EventProvider::notifyRecordInserted(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    m_recordInserted.notify(queryContext, key, value);
}

void EventProvider::notifyRecordRemoved(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    m_recordRemoved.notify(queryContext, key);
}

} // namespace nx::clusterdb::map
