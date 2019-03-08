#include "object_destruction_flag.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

namespace nx::utils {

InterruptionFlag::InterruptionFlag()
{
    m_watcherStates.reserve(1);
}

InterruptionFlag::~InterruptionFlag()
{
    interrupt();
}

void InterruptionFlag::interrupt()
{
    NX_ASSERT(m_watcherStates.empty() || m_lastWatchingThreadId == std::this_thread::get_id());

    std::for_each(
        m_watcherStates.begin(), m_watcherStates.end(),
        [](bool* value) { *value = true; });
    m_watcherStates.clear();
}

//-------------------------------------------------------------------------------------------------

InterruptionFlag::Watcher::Watcher(InterruptionFlag* const flag):
    m_objectDestructionFlag(flag)
{
    NX_ASSERT(
        m_objectDestructionFlag->m_watcherStates.empty() ||
        m_objectDestructionFlag->m_lastWatchingThreadId == std::this_thread::get_id());

    m_objectDestructionFlag->m_lastWatchingThreadId = std::this_thread::get_id();

    m_objectDestructionFlag->m_watcherStates.push_back(&m_interrupted);
}

InterruptionFlag::Watcher::~Watcher()
{
    if (m_interrupted)
        return; //< Object has been destroyed and m_objectDestructionFlag is invalid.

    NX_ASSERT(m_objectDestructionFlag->m_watcherStates.back() == &m_interrupted);
    m_objectDestructionFlag->m_watcherStates.pop_back();
}

bool InterruptionFlag::Watcher::interrupted() const
{
    return m_interrupted;
}

} // namespace nx::utils
