// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "interruption_flag.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

namespace nx::utils {

InterruptionFlag::InterruptionFlag() = default;

InterruptionFlag::~InterruptionFlag()
{
    interrupt();
}

void InterruptionFlag::interrupt()
{
    NX_ASSERT(
        m_watchers.empty() || m_lastWatchingThreadId == std::this_thread::get_id(),
        "m_watchers.size(): %1, m_lastWatchingThreadId: %2, std::this_thread id: %3, m_watchers: %4",
        m_watchers.size(), m_lastWatchingThreadId, std::this_thread::get_id(), *this);

    std::for_each(
        m_watchers.begin(), m_watchers.end(),
        [](Watcher* watcher) { watcher->m_interrupted = true; });
    m_watchers.clear();
}

void InterruptionFlag::push(Watcher* watcher)
{
    NX_ASSERT(
        m_watchers.empty() || m_lastWatchingThreadId == std::this_thread::get_id(),
        "m_watchers.size(): %1, m_lastWatchingThreadId: %2, std::this_thread id: %3, m_watchers: %4",
        m_watchers.size(), m_lastWatchingThreadId, std::this_thread::get_id(), *this);

    m_lastWatchingThreadId = std::this_thread::get_id();
    m_watchers.push_back(watcher);
}

void InterruptionFlag::pop(Watcher* watcher)
{
    NX_ASSERT(
        m_watchers.back() == watcher,
        "Expected watcher %1, got: %2", *watcher, *m_watchers.back());
    m_watchers.pop_back();
}

//-------------------------------------------------------------------------------------------------

InterruptionFlag::Watcher::Watcher(
    InterruptionFlag* const flag,
    std::source_location location)
    :
    m_objectDestructionFlag(flag),
    m_location(std::move(location))
{
    m_objectDestructionFlag->push(this);
}

InterruptionFlag::Watcher::~Watcher()
{
    if (m_interrupted)
        return; //< Object has been destroyed and m_objectDestructionFlag is invalid.

    m_objectDestructionFlag->pop(this);
}

bool InterruptionFlag::Watcher::interrupted() const
{
    return m_interrupted;
}

std::string InterruptionFlag::Watcher::toString() const
{
    std::stringstream str;

    const char* interrupted = m_interrupted ? "true" : "false";
    str << "{interrupted: " << interrupted << ", func: " << m_location.function_name() << "}";

    return str.str();
}

std::string InterruptionFlag::toString() const
{
    std::stringstream str;

    str << "[";
    for (int i = 0; i < (int) m_watchers.size(); ++i)
    {
        str << m_watchers[i]->toString();
        if (i < (int) m_watchers.size() - 1)
            str << ", ";
    }
    str << "]";

    return str.str();
}

} // namespace nx::utils
