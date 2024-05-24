// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "combined_id.h"

namespace nx::vms::api::combined_id {

std::optional<int> findIdSeparator(const QString& id, char separator)
{
    const auto p = id.indexOf(separator);
    if (p <= 0)
        return std::nullopt;

    return p;
}

nx::Uuid localIdFromCombined(const QString& id)
{
    if (id.isEmpty())
        return {};

    const auto p = findIdSeparator(id, '_');
    if (!p)
        return nx::Uuid::fromStringSafe(id);

    return nx::Uuid::fromStringSafe(id.mid(0, *p));
}

nx::Uuid serverIdFromCombined(const QString& id)
{
    if (id.isEmpty())
        return {};

    const auto from = findIdSeparator(id, '_');
    if (!from)
        return {};

    return nx::Uuid::fromStringSafe(id.mid(*from + 1));
}

nx::Uuid serverIdFromCombined(const QString& id, char endSeparator)
{
    if (id.isEmpty())
        return {};

    const auto from = findIdSeparator(id, '_');
    if (!from)
        return {};

    const auto to = findIdSeparator(id, endSeparator);
    return nx::Uuid::fromStringSafe(id.mid(*from + 1, to ? *to : -1));
}

QString combineId(const nx::Uuid& localId, const nx::Uuid& serverId)
{
    return localId.toSimpleString() + '_' + serverId.toSimpleString();
}

} // namespace nx::vms::api::combined_id
