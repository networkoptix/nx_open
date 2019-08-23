#pragma once

#include <set>
#include <optional>
#include "QtCore/QString"

#include <QtCore/QString>

namespace nx::analytics {

std::optional<std::set<QString>> mergeEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second);

std::optional<std::set<QString>> intersectEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second);

} // namespace nx::analytics
