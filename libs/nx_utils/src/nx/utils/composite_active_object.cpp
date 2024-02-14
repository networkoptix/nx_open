// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iterator>

#include "composite_active_object.h"

namespace nx::utils {

// -------------------------------------------------------------------------------------------------
// CompositeActiveObject implementation.

CompositeActiveObject::CompositeActiveObject(bool syncTermination, bool clearOnExit) noexcept:
    m_synchronous(syncTermination),
    m_clearOnExit(clearOnExit)
{
}

CompositeActiveObject::~CompositeActiveObject() noexcept
{
    if (m_clearOnExit)
    {
        try
        {
            clear();
        }
        catch (...)
        {
        }
    }

    try
    {
        clearChildren();
    }
    catch (...)
    {
    }
}

void CompositeActiveObject::activateObjectHook()
{
    static const char* FUN = "CompositeActiveObject::activateObjectHook()";

    auto it = m_childObjects.begin();

    try
    {
        for (; it != m_childObjects.end(); ++it)
        {
            (*it)->activateObject();
        }
    }
    catch (const std::exception& e)
    {
        std::string resultExceptionMessage(FUN);
        resultExceptionMessage += ": ";
        resultExceptionMessage += e.what();

        try
        {
            m_state = (int)ActiveState::deactivating;
            ActiveObjectArray::reverse_iterator rit(it);
            deactivateObjects(rit);
            waitForSomeObjects(rit, m_childObjects.rend());
        }
        catch (const std::exception& e)
        {
            resultExceptionMessage += "; ";
            resultExceptionMessage += e.what();
        }

        m_state = (int)ActiveState::notActive;

        throw std::runtime_error(resultExceptionMessage);
    }
}

void CompositeActiveObject::deactivateObjectHook()
{
    deactivateObjects(m_childObjects.rbegin());
}

void CompositeActiveObject::waitObjectHook()
{
    ActiveObjectArray copyOfChildObjects;

    {
        const std::lock_guard<std::mutex> guard(m_lock);
        copyOfChildObjects = m_childObjects;
    }

    waitForSomeObjects(copyOfChildObjects.rbegin(), copyOfChildObjects.rend());
}

void CompositeActiveObject::addChildObject(const std::shared_ptr<ActiveObject>& child,
    bool addToHead)
{
    const std::lock_guard<std::mutex> guard(m_lock);

    if(m_state == (int)ActiveState::active)
    {
        if(!child->active())
            child->activateObject();
    }
    else
    {
        if (child->active())
        {
            child->deactivateObject();
            child->waitObject();
        }
    }

    std::inserter(m_childObjects,
        addToHead ? m_childObjects.begin() : m_childObjects.end()) =
        std::shared_ptr<ActiveObject>(child);
}

void CompositeActiveObject::clearChildren()
{
    const std::lock_guard<std::mutex> guard(m_lock);

    if (m_state != (int)ActiveState::notActive)
    {
        ActiveObjectArray::reverse_iterator rit(m_childObjects.rbegin());
        deactivateObjects(rit);
        waitForSomeObjects(rit, m_childObjects.rend());
        m_state = (int)ActiveState::notActive;
    }

    m_childObjects.clear();
}

void CompositeActiveObject::waitForSomeObjects(ActiveObjectArray::reverse_iterator rit,
    ActiveObjectArray::reverse_iterator rend)
{
    static const char* FUN = "CompositeActiveObject::waitForSomeObjects()";

    bool hasError = false;
    std::string resultErrors;

    for (; rit != rend; ++rit)
    {
        try
        {
            (*rit)->waitObject();
        }
        catch (const std::exception& ex)
        {
            hasError = true;
            resultErrors += ex.what();
            resultErrors += "; ";
        }
    }

    if (hasError)
    {
        throw std::runtime_error(
            std::string(FUN) + ": Can't wait child active object. Caught exception: " +
            resultErrors);
    }
}

void CompositeActiveObject::deactivateObjects(ActiveObjectArray::reverse_iterator rit)
{
    static const char* FUN = "CompositeActiveObject:deactivateObjects()";

    bool hasError = false;
    std::string resultErrors;

    for (; rit != m_childObjects.rend(); ++rit)
    {
        try
        {
            (*rit)->deactivateObject();

            if (m_synchronous)
            {
                (*rit)->waitObject();
            }
        }
        catch (const std::exception& ex)
        {
            hasError = true;
            resultErrors += ex.what();
            resultErrors += "; ";
        }
    }

    if (hasError)
    {
        throw std::runtime_error(
            std::string(FUN) + ": Can't deactivate child active object. Caught exception: " +
            resultErrors);
    }
}

void CompositeActiveObject::clear()
{
    const std::lock_guard<std::mutex> guard(m_lock);

    for (auto& childObject : m_childObjects)
    {
        childObject->clear();
    }
}

} // namespace nx::utils
