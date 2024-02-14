// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <deque>
#include <functional>

#include "active_object.h"

namespace nx::utils {

class NX_UTILS_API CompositeActiveObject: public SimpleActiveObject
{
public:
    CompositeActiveObject(
      bool syncTermination = false,
      bool clearOnExit = true)
      noexcept;

    /**
     * Perform deactivating all owned objects, and waits for
     * its completion.
     */
    virtual ~CompositeActiveObject() noexcept;

    /**
     * Calls clear() for all owned objects
     */
    virtual void clear() override;

    /**
     * Deactivate and wait for stop for all owned Active Objects.
     * Clears list of the objects.
     */
    void clearChildren();

    void addChildObject(const std::shared_ptr<ActiveObject>& child, bool addToHead = false);

protected:
    using ActiveObjectArray = std::deque<std::shared_ptr<ActiveObject>>;

protected:
    virtual void activateObjectHook() override;

    virtual void deactivateObjectHook() override;

    virtual void waitObjectHook() override;

    /**
     * Thread-unsafe methods.
     */
    void waitForSomeObjects(ActiveObjectArray::reverse_iterator rbegin,
        ActiveObjectArray::reverse_iterator rend);

    void
    deactivateObjects(ActiveObjectArray::reverse_iterator rit);

protected:
    const bool m_synchronous;
    const bool m_clearOnExit;
    ActiveObjectArray m_childObjects;
};

} // namespace nx::utils
