// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <optional>

#include <QtCore/QString>

namespace nx::analytics {

NX_VMS_COMMON_API std::optional<std::set<QString>> mergeEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second);

std::optional<std::set<QString>> intersectEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second);

} // namespace nx::analytics
