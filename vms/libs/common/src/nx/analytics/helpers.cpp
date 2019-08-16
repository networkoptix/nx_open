#include "helpers.h"

namespace nx::analytics {

std::optional<std::set<QString>> mergeEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<QString> result = *first;
    result.insert(second->begin(), second->end());
    return result;
}

std::optional<std::set<QString>> intersectEntityIds(
    const std::set<QString>* first,
    const std::set<QString>* second)
{
    if (!first && !second)
        return std::nullopt;

    if (!first)
        return *second;

    if (!second)
        return *first;

    std::set<QString> result;
    std::set_intersection(first->begin(), first->end(), second->begin(), second->end(),
        std::inserter(result, result.end()));

    return result;
}

} // namespace nx::analytics
