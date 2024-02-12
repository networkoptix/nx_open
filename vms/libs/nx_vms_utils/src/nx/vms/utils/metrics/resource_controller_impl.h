// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    using ResourceController::add;
    ResourceType* add(ResourceType resource, QString id, Scope scope);
    ResourceType* add(ResourceType resource, const nx::Uuid& id, Scope scope);

    using ResourceController::remove;
    bool remove(const nx::Uuid& id);

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
    const auto resourceRules = rules();
    api::metrics::ResourceManifest manifest{name(), resourceRules.name};
    manifest.resource = resourceRules.resource;
    manifest.values = m_provider->manifest();
    for (const auto& [groupId, groupRules]: resourceRules.values)
    {
        const auto groupIt = std::find_if(
            manifest.values.begin(), manifest.values.end(),
            [id = &groupId](const auto& g) { return g.id == *id; });
        if (!NX_ASSERT(groupIt != manifest.values.end(), "Group not found: %1", groupId))
            continue;

        if (!groupRules.name.isEmpty())
            groupIt->name = groupRules.name;
        for (const auto& [valueId, valueRule]: groupRules.values)
        {
            const auto existing = std::find_if(
                groupIt->values.begin(), groupIt->values.end(),
                [id = &valueId](const auto& m) { return m.id == *id; });
            if (existing != groupIt->values.end())
            {
                // Override existing value manifest.
                api::metrics::apply(valueRule, &*existing);
                continue;
            }

            // TODO: Use proper insert rules.
            const auto position = std::find_if(
                groupIt->values.begin(), groupIt->values.end(),
                [r = &valueRule](const auto& m) { return m.id == r->insert; });
            api::metrics::ValueManifest newManifest(valueId, valueRule.name);
            api::metrics::apply(valueRule, &newManifest);
            groupIt->values.insert(position, std::move(newManifest));
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
    ResourceType resource, const nx::Uuid& id, Scope scope)
{
    return add(std::move(resource), id.toSimpleString(), scope);
}

template<typename ResourceType>
bool ResourceControllerImpl<ResourceType>::remove(const nx::Uuid& id)
{
    return remove(id.toSimpleString());
}

} // namespace nx::vms::utils::metrics
