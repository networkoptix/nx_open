// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "active_object.h"

#include <iostream>

#include <nx/utils/log/log.h>

namespace nx::utils {

//-------------------------------------------------------------------------------------------------
// SimpleActiveObject implementation.

SimpleActiveObject::SimpleActiveObject():
    m_state((int)ActiveState::notActive)
{
}

SimpleActiveObject::~SimpleActiveObject() noexcept
{
    if (m_state != (int)ActiveState::notActive)
    {
        NX_ERROR(this) << "Destroyed non deactivated ActiveObject.";
    }
}

void SimpleActiveObject::activateObject()
{
    {
        const std::lock_guard<std::mutex> guard(m_lock);

        if (m_state == (int)ActiveState::notActive)
        {
            activateObjectHook();
            m_state = (int)ActiveState::active;
            return;
        }
    }

    NX_ERROR(this) << "Already active.";
}

void SimpleActiveObject::deactivateObject()
{
    const std::lock_guard<std::mutex> guard(m_lock);

    if (m_state != (int)ActiveState::active)
    {
        return;
    }

    m_state = (int)ActiveState::deactivating;
    m_condition.notify_all();

    try
    {
        deactivateObjectHook();
    }
    catch (...)
    {
        m_state = (int)ActiveState::active;
        throw;
    }
}

void SimpleActiveObject::waitObject()
{
    {
        std::unique_lock<std::mutex> guard(m_lock);

        while (m_state == (int)ActiveState::active || waitMoreHook())
        {
            m_condition.wait(guard);
        }
    }

    waitObjectHook();

    const std::lock_guard<std::mutex> guard(m_lock);

    if (m_state == (int)ActiveState::deactivating)
    {
        m_state = (int)ActiveState::notActive;
    }
}

bool SimpleActiveObject::active()
{
    return m_state == (int)ActiveState::active;
}

void SimpleActiveObject::activateObjectHook()
{
}

void SimpleActiveObject::deactivateObjectHook()
{
}

void SimpleActiveObject::waitObjectHook()
{
}

bool SimpleActiveObject::waitMoreHook()
{
    return false;
}

} // namespace nx::utils
