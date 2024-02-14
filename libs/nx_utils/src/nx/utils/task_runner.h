// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>

#include <nx/utils/move_only_func.h>

#include "active_object.h"

namespace nx::utils {

/** Performs tasks in several threads parallelly.
 */
class NX_UTILS_API TaskRunner: public SimpleActiveObject
{
public:
    using Task = nx::utils::MoveOnlyFunc<void()>;

    TaskRunner(int threadsNumber);

    virtual ~TaskRunner() noexcept;

    void runTask(Task task);

    void clear();

protected:
    void activateObjectHook() override;

    void deactivateObjectHook() override;

    void waitObjectHook() override;

private:
    struct Impl;

    const int m_threadsNumber;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nx::utils
