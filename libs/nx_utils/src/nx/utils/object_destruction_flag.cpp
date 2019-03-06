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
    std::for_each(
        m_watcherStates.begin(), m_watcherStates.end(),
        [](ControlledObjectState* value) { *value = ControlledObjectState::deleted; });
}

void ObjectDestructionFlag::markAsDeleted()
{
    std::for_each(
        m_watcherStates.begin(), m_watcherStates.end(),
        [](ControlledObjectState* value) { *value = ControlledObjectState::markedForDeletion; });
}

//-------------------------------------------------------------------------------------------------

ObjectDestructionFlag::Watcher::Watcher(ObjectDestructionFlag* const flag):
    m_localValue(ControlledObjectState::alive),
    m_objectDestructionFlag(flag)
{
    m_objectDestructionFlag->m_watcherStates.push_back(&m_localValue);
}

ObjectDestructionFlag::Watcher::~Watcher()
{
    if (m_localValue == ControlledObjectState::deleted)
        return; //< Object has been destroyed and m_objectDestructionFlag is invalid.

    NX_ASSERT(m_objectDestructionFlag->m_watcherStates.back() == &m_localValue);
    m_objectDestructionFlag->m_watcherStates.pop_back();
}

bool ObjectDestructionFlag::Watcher::objectDestroyed() const
{
    return m_localValue > ControlledObjectState::alive;
}

} // namespace nx::utils
