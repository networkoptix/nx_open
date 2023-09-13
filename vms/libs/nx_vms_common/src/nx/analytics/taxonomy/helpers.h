// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QString>

#include <nx/analytics/taxonomy/abstract_state.h>

namespace nx::analytics::taxonomy {

NX_VMS_COMMON_API std::set<QString> getAllDerivedTypeIds(
    const AbstractState* state, const QString& typeId);

NX_VMS_COMMON_API std::set<QString> getAllBaseTypeIds(
    const AbstractState* state, const QString& typeId);

NX_VMS_COMMON_API bool isBaseType(
    const AbstractState* state, const QString& baseTypeId, const QString& derivedTypeId);

} // namespace nx::analytics::taxonomy
