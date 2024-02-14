// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace nx::utils {

class NX_UTILS_API ActiveObject
{
public:
    virtual ~ActiveObject() noexcept = default;

    virtual void activateObject() = 0;

    virtual void deactivateObject() = 0;

    virtual void waitObject() = 0;

    virtual bool active() = 0;

    virtual void clear()
    {
    }

protected:
    enum class ActiveState: int
    {
        active = 0,
        deactivating,
        notActive
    };
};

/**
 * ActiveObject state machine implementation, provides hooks for additional state changing
 * operations.
 */
class NX_UTILS_API SimpleActiveObject: public ActiveObject
{
public:
    SimpleActiveObject();

    virtual ~SimpleActiveObject() noexcept;

    virtual void activateObject() override;

    virtual void deactivateObject() override;

    virtual void waitObject() override;

    virtual bool active() override;

protected:
    virtual void activateObjectHook();

    virtual void deactivateObjectHook();

    virtual bool waitMoreHook();

    virtual void waitObjectHook();

protected:
    std::mutex m_lock;
    std::condition_variable m_condition;
    volatile std::atomic<int> m_state;
};

} // namespace nx::utils
