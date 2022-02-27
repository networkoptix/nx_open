// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "scope_guard.h"

namespace nx::utils {

/**
 * Scoped variable rollback based on nx::utils::ScopeGuard, an alternative to QScopedValueRollback.
 * Unlike the latter it has 'fire' method for early rollback, and being constructed via template
 * function it doesn't require explicit template specialization. For rollback cancellation it has
 * 'disarm' method (an analog of QScopedValueRollback::commit).
 *
 * Simple usage:
 *    const auto variableRollback = nx::utils::makeScopedRollback(variable);
 * or
 *    const auto variableRollback = nx::utils::makeScopedRollback(variable, newValue);
 *
 * More complicated usage:
 *    auto variableRollback = nx::utils::makeScopedRollback(variable, newValue);
 *    ...
 *    if (someCondition)
 *        variableRollback.disarm();
 *    ...
 *    variableRollback.fire();
 *    ...
 */

template<class T>
auto makeScopedRollback(T& var)
{
    return makeScopeGuard([&var, oldValue = var]() { var = oldValue; });
}

template<class T>
auto makeScopedRollback(T& var, const T& newValue)
{
    auto result = makeScopedRollback(var);
    var = newValue;
    return result;
}

} // namespace nx::utils
