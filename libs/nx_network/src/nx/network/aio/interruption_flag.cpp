#include "interruption_flag.h"

namespace nx::network::aio {

InterruptionFlag::ScopeWatcher::ScopeWatcher(
    aio::BasicPollable* aioObject,
    InterruptionFlag* flag)
    :
    m_destructionWatcher(&flag->m_destructionFlag),
    m_aioObject(aioObject),
    m_aioThread(aioObject->getAioThread())
{
}

bool InterruptionFlag::ScopeWatcher::interrupted() const
{
    return stateChange() != StateChange::noChange;
}

InterruptionFlag::StateChange InterruptionFlag::ScopeWatcher::stateChange() const
{
    if (m_destructionWatcher.objectDestroyed())
        return StateChange::thisDeleted;
    if (m_aioObject->getAioThread() != m_aioThread)
        return StateChange::aioThreadChanged;
    return StateChange::noChange;
}

} // namespace nx::network::aio
