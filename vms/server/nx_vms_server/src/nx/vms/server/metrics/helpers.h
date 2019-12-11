#pragma once

#include <core/resource/resource.h>
#include <nx/vms/utils/metrics/resource_provider.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

template<typename ResourceType>
bool isServerOffline(const ResourceType& r)
{
    const auto server = r->commonModule()->resourcePool()->getResourceById(r->getParentId());
    return server->getStatus() == Qn::Offline;
}

template<typename ResourceType>
utils::metrics::Watch<ResourceType> staticWatch()
{
    return
        [](const ResourceType& /*resource*/, utils::metrics::OnChange /*change*/)
        {
            // Newer notifies.
            return nx::utils::SharedGuardPtr();
        };
}

template<typename ResourceType, typename Signal>
utils::metrics::Watch<ResourceType> qtSignalWatch(Signal signal)
{
    return
        [signal](const ResourceType& resource, utils::metrics::OnChange change)
        {
            const auto connection = QObject::connect(
                resource, signal, [change = std::move(change)](const auto&) { change(); });

            return nx::utils::makeSharedGuard([connection]() { QObject::disconnect(connection); });
        };
}

template<typename ResourceType, typename... SignalList>
utils::metrics::Watch<ResourceType> qtSignalWatch(SignalList... signalList)
{
    return
        [=](const ResourceType& resource, utils::metrics::OnChange change)
        {
            const auto sharedChange = std::make_shared<utils::metrics::OnChange>(std::move(change));
            std::vector<QMetaObject::Connection> connections;
            (connections.push_back(QObject::connect(
                resource, signalList, [sharedChange](const auto&) { (*sharedChange)(); })), ...);

            return nx::utils::makeSharedGuard(
                [connections = std::move(connections)]()
                {
                    for (const auto& connection: connections)
                        QObject::disconnect(connection);
                });
        };
}

template<typename ResourceType>
utils::metrics::Watch<ResourceType> propertyWatch(const QString& property)
{
    return
        [property](const ResourceType& resource, utils::metrics::OnChange change)
        {
            const auto connection = QObject::connect(
                resource, &QnResource::propertyChanged,
                [change = std::move(change), property = std::move(property)](
                    const auto& /*resource*/, const QString& key)
                {
                    if (key == property)
                        change();
                });

            return nx::utils::makeSharedGuard([connection]() { QObject::disconnect(connection); });
        };
}

nx::utils::SharedGuardPtr makeTimer(
    nx::utils::TimerManager* timerManager,
    std::chrono::milliseconds timeout,
    utils::metrics::OnChange change);

/*
 * @param speedRate speedup all timers in N times. Used for UT only.
 */
void setTimerMultiplier(int speedRate);

template<typename ResourceType>
utils::metrics::Watch<ResourceType> timerWatch(std::chrono::milliseconds timeout)
{
    return
        [timeout](const ResourceType& resource, utils::metrics::OnChange change)
        {
            return makeTimer(resource->commonModule()->timerManager(), timeout, std::move(change));
        };
}

} // namespace nx::vms::server::metrics
