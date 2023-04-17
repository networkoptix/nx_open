// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <nx/utils/log/assert.h>
#include <nx/utils/move_only_func.h>

#include "basic_pollable.h"

namespace nx::network::aio {

/**
 * nx::network::aio::BasicPollable implementation that offers some convenience for managing
 * dependant nx::network::aio::BasicPollable objects.
 */
template<typename Base>
requires std::is_base_of_v<nx::network::aio::BasicPollable, Base>
class PollbableWithDependants:
    public Base
{
    using base_type = Base;

public:
    using base_type::base_type;

    ~PollbableWithDependants()
    {
        // Dependants should have been cancelled within the AIO thread.
        NX_ASSERT(m_dependants.empty(), "pleaseStop or cancelDependantsIo must have been invoked");

        if (m_onDestruction)
            nx::utils::swapAndCall(m_onDestruction);
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        for (auto& depCtx: m_dependants)
            depCtx.obj->bindToAioThread(aioThread);
    }

    /**
     * @param handler Guaranteed to be executed within AIO thread (within either
     * stopWhileInAioThread() or the destructor).
     */
    void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onDestruction = std::move(handler);
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        cancelDependantsIo();

        if (m_onDestruction)
            nx::utils::swapAndCall(m_onDestruction);
    }

    /**
     * Tells about an existing dependant object.
     * The dependant will be
     * - bound to new AIO thread when latter is changed
     * - asked to cancel its I/O when this is doing that
     * @param customStopFunc if provided will be invoked from AIO thread to cancel
     * all scheduled async I/O. If not specified, `obj->pleaseStopSync()` will be invoked.
     */
    void addDependant(
        nx::network::aio::BasicPollable* dependant,
        nx::utils::MoveOnlyFunc<void()> customStopFunc = nullptr)
    {
        dependant->bindToAioThread(this->getAioThread());

        m_dependants.push_back({dependant, std::move(customStopFunc)});
    }

    /**
     * Cancels dependants' scheduled I/O by invoking either customStopFunc or pleaseStopSync().
     * It is safe to invoke this method multiple times.
     */
    void cancelDependantsIo()
    {
        if (m_dependants.empty())
            return;

        NX_ASSERT(this->isInSelfAioThread());

        for (auto& depCtx: m_dependants)
        {
            if (depCtx.customStopFunc)
                depCtx.customStopFunc();
            else
                depCtx.obj->pleaseStopSync();
        }

        m_dependants.clear();
    }

private:
    struct DependantCtx
    {
        nx::network::aio::BasicPollable* obj = nullptr;
        nx::utils::MoveOnlyFunc<void()> customStopFunc;
    };

    nx::utils::MoveOnlyFunc<void()> m_onDestruction;
    std::vector<DependantCtx> m_dependants;
};

} // namespace nx::network::aio
