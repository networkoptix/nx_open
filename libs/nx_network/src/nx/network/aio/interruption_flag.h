#pragma once

#include <nx/utils/object_destruction_flag.h>

#include "basic_pollable.h"

namespace nx::network::aio {

class NX_NETWORK_API InterruptionFlag
{
public:
    enum class StateChange
    {
        noChange,
        aioThreadChanged,
        thisDeleted,
    };

    class NX_NETWORK_API ScopeWatcher
    {
    public:
        ScopeWatcher(
            aio::BasicPollable* aioObject,
            InterruptionFlag* flag);

        ScopeWatcher(ScopeWatcher&&) = default;
        ScopeWatcher& operator=(ScopeWatcher&&) = default;

        bool interrupted() const;

        StateChange stateChange() const;

    private:
        nx::utils::ObjectDestructionFlag::Watcher m_destructionWatcher;
        aio::BasicPollable* m_aioObject = nullptr;
        AbstractAioThread* m_aioThread = nullptr;
    };

private:
    nx::utils::ObjectDestructionFlag m_destructionFlag;
};

} // namespace nx::network::aio
