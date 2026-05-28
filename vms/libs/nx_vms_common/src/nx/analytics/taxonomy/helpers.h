// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/analytics/taxonomy/abstract_state.h>

namespace nx::analytics::taxonomy {

NX_VMS_COMMON_API std::set<std::string> getAllDerivedTypeIds(
    const AbstractState* state, const std::string& typeId);

NX_VMS_COMMON_API std::set<std::string> getAllBaseTypeIds(
    const AbstractState* state, const std::string& typeId);

NX_VMS_COMMON_API bool isBaseType(
    const AbstractState* state, const std::string& baseTypeId, const std::string& derivedTypeId);

NX_VMS_COMMON_API bool isTypeOrSubtypeOf(
    AbstractState* state, const std::string& typeId, const std::string& targetTypeId);

} // namespace nx::analytics::taxonomy
