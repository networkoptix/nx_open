#pragma once

#include <nx/utils/uuid.h>

#include "resource_controller.h"

namespace nx::vms::utils::metrics {

/**
 * Inherit to implement controller for a specific type.
 */
template<typename ResourceType>
class ResourceControllerImpl: public ResourceController
{
public:
    using Value = api::metrics::Value;
    using Resource = ResourceType;

    ResourceControllerImpl(QString id, ValueGroupProviders<ResourceType> providers);
    virtual api::metrics::ResourceManifest manifest() const override final;

protected:
    ResourceType* add(ResourceType resource, QString id, Scope scope);
    ResourceType* add(ResourceType resource, QnUuid id, Scope scope);
    bool remove(QnUuid id);

private:
    std::unique_ptr<ResourceProvider<Resource>> m_provider;
};

// ------------------------------------------------------------------------------------------------

template<typename ResourceType>
ResourceControllerImpl<ResourceType>::ResourceControllerImpl(
    QString id, ValueGroupProviders<ResourceType> providers)
:
    ResourceController(std::move(id)),
    m_provider(std::make_unique<ResourceProvider<ResourceType>>(std::move(providers)))
{
}

template<typename ResourceType>
api::metrics::ResourceManifest ResourceControllerImpl<ResourceType>::manifest() const
{
    auto manifest = m_provider->manifest();
    for (const auto& [groupId, groupRules]: rules())
    {
        const auto groupIt = std::find_if(
            manifest.begin(), manifest.end(),
            [id = &groupId](const auto& g) { return g.id == *id; });
        if (!NX_ASSERT(groupIt != manifest.end(), "Group not found: %1", groupId))
            continue;

        groupIt->name = groupRules.name;
        for (const auto& [valueId, valueRule]: groupRules.values)
        {
            const auto existing = std::find_if(
                groupIt->values.begin(), groupIt->values.end(),
                [id = &valueId](const auto& m) { return m.id == *id; });
            if (existing != groupIt->values.end())
            {
                // Override existing value manifest.
                if (!valueRule.name.isEmpty())
                    existing->name = valueRule.name;
                existing->display = valueRule.display;
                existing->unit = valueRule.unit;
                continue;
            }

            // TODO: Use proper insert rules.
            const auto position = std::find_if(
                groupIt->values.begin(), groupIt->values.end(),
                [r = &valueRule](const auto& m) { return m.id == r->insert; });
            groupIt->values.insert(position, api::metrics::ValueManifest{
                {valueId, valueRule.name}, valueRule.display, valueRule.unit});
        }
    }

    return manifest;
}

template<typename ResourceType>
ResourceType* ResourceControllerImpl<ResourceType>::add(
    ResourceType resource, QString id, Scope scope)
{
    auto description = std::make_unique<utils::metrics::TypedResourceDescription<Resource>>(
        std::move(resource), std::move(id), scope);

    const auto ptr = &description->resource;
    ResourceController::add(m_provider->monitor(std::move(description)));
    return ptr;
}

template<typename ResourceType>
ResourceType* ResourceControllerImpl<ResourceType>::add(
    ResourceType resource, QnUuid id, Scope scope)
{
    return add(std::move(resource), id.toSimpleString(), scope);
}

template<typename ResourceType>
bool ResourceControllerImpl<ResourceType>::remove(QnUuid id)
{
    return ResourceController::remove(id.toSimpleString());
}

} // namespace nx::vms::utils::metrics
