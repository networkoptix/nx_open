#pragma once

#include <QtCore/QList>

#include <nx/analytics/types.h>

namespace nx::analytics {

template <typename Descriptor, typename Item>
std::map<QString, Descriptor> fromManifestItemListToDescriptorMap(
    const nx::analytics::EngineId& engineId, const QList<Item>& items)
{
    std::map<QString, Descriptor> result;
    for (const auto& item : items)
    {
        auto descriptor = Descriptor(engineId, item);
        result.emplace(descriptor.getId(), std::move(descriptor));
    }

    return result;
}

} // namespace nx::analytics
