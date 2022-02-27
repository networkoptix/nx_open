// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "interruption_flag.h"

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

void InterruptionFlag::pushWatcherState(bool* watcherState)
{
    NX_ASSERT(
        m_watcherStates.empty() ||
        m_lastWatchingThreadId == std::this_thread::get_id());

    m_lastWatchingThreadId = std::this_thread::get_id();
    m_watcherStates.push_back(watcherState);
}

void InterruptionFlag::popWatcherState(bool* watcherState)
{
    NX_ASSERT(m_watcherStates.back() == watcherState);
    m_watcherStates.pop_back();
}

//-------------------------------------------------------------------------------------------------

InterruptionFlag::Watcher::Watcher(InterruptionFlag* const flag):
    m_objectDestructionFlag(flag)
{
    m_objectDestructionFlag->pushWatcherState(&m_interrupted);
}

InterruptionFlag::Watcher::~Watcher()
{
    if (m_interrupted)
        return; //< Object has been destroyed and m_objectDestructionFlag is invalid.

    m_objectDestructionFlag->popWatcherState(&m_interrupted);
}

bool InterruptionFlag::Watcher::interrupted() const
{
    return m_interrupted;
}

} // namespace nx::utils
