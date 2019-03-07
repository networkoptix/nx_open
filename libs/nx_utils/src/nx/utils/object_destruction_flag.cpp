#include "object_destruction_flag.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

namespace nx::utils {

ObjectDestructionFlag::ObjectDestructionFlag()
{
    m_watcherStates.reserve(1);
}

ObjectDestructionFlag::~ObjectDestructionFlag()
{
    recordCustomState(ControlledObjectState::deleted);
}

void ObjectDestructionFlag::markAsDeleted()
{
    recordCustomState(ControlledObjectState::markedForDeletion);
}

void ObjectDestructionFlag::recordCustomState(ControlledObjectState state)
{
    NX_ASSERT(m_watcherStates.empty() || m_lastWatchingThreadId == std::this_thread::get_id());

    std::for_each(
        m_watcherStates.begin(), m_watcherStates.end(),
        [state](ControlledObjectState* value) { *value = state; });
    m_watcherStates.clear();
}

//-------------------------------------------------------------------------------------------------

ObjectDestructionFlag::Watcher::Watcher(ObjectDestructionFlag* const flag):
    m_localValue(ControlledObjectState::alive),
    m_objectDestructionFlag(flag)
{
    if (!m_objectDestructionFlag->m_watcherStates.empty())
        NX_ASSERT(m_objectDestructionFlag->m_lastWatchingThreadId == std::this_thread::get_id());

    m_objectDestructionFlag->m_lastWatchingThreadId = std::this_thread::get_id();

    m_objectDestructionFlag->m_watcherStates.push_back(&m_localValue);
}

ObjectDestructionFlag::Watcher::~Watcher()
{
    if (m_localValue != ControlledObjectState::alive)
        return; //< Object has been destroyed and m_objectDestructionFlag is invalid.

    NX_ASSERT(m_objectDestructionFlag->m_watcherStates.back() == &m_localValue);
    m_objectDestructionFlag->m_watcherStates.pop_back();
}

bool ObjectDestructionFlag::Watcher::interrupted() const
{
    return m_localValue != ControlledObjectState::alive;
}

bool ObjectDestructionFlag::Watcher::objectDestroyed() const
{
    return m_localValue != ControlledObjectState::alive;
}

} // namespace nx::utils
