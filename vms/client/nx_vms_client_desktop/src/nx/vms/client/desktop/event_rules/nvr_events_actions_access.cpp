#include "nvr_events_actions_access.h"

#include <nx/vms/event/events/abstract_event.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

using namespace nx::vms::api;

namespace {

bool serversHaveServerFlag(const QnMediaServerResourceList& servers, ServerFlag serverFlag)
{
    return std::any_of(servers.cbegin(), servers.cend(),
        [serverFlag](const QnMediaServerResourcePtr& server)
        {
            return server->getServerFlags().testFlag(serverFlag);
        });
}

}

namespace nx::vms::client::desktop {

QList<EventType> NvrEventsActionsAccess::removeInacessibleNvrEvents(
    const QList<EventType>& events,
    QnResourcePool* resourcePool)
{
    static const QHash<EventType, ServerFlag> flagsRequiredForEvent = {
        {EventType::poeOverBudgetEvent, ServerFlag::SF_HasPoeManagementCapability},
        {EventType::fanErrorEvent, ServerFlag::SF_HasFanMonitoringCapability}};

    const auto servers = resourcePool->getAllServers(Qn::AnyStatus);

    QList<EventType> result;

    std::copy_if(events.cbegin(), events.cend(), std::back_inserter(result),
        [&servers](nx::vms::api::EventType eventType)
        {
            return !flagsRequiredForEvent.contains(eventType)
                || serversHaveServerFlag(servers, flagsRequiredForEvent.value(eventType));
        });

    return result;
}

QList<ActionType> NvrEventsActionsAccess::removeInacessibleNvrActions(
    const QList<ActionType>& actions,
    QnResourcePool* resourcePool)
{
    static const QHash<ActionType, ServerFlag> flagsRequiredForAction = {
        {ActionType::buzzerAction, ServerFlag::SF_HasBuzzer}};

    const auto servers = resourcePool->getAllServers(Qn::AnyStatus);

    QList<ActionType> result;

    std::copy_if(actions.cbegin(), actions.cend(), std::back_inserter(result),
        [&servers](nx::vms::api::ActionType actionType)
        {
            return !flagsRequiredForAction.contains(actionType)
                || serversHaveServerFlag(servers, flagsRequiredForAction.value(actionType));
        });

    return result;
}

} // namespace nx::vms::client::desktop
