#pragma once

#include <nx/utils/scope_guard.h>

#include "data_base.h"

namespace nx::vms::utils::metrics {

template<typename Resource>
class AbstractParameterProvider
{
public:
    AbstractParameterProvider(api::metrics::ParameterGroupManifest manifest);
    virtual ~AbstractParameterProvider() = default;

    const api::metrics::ParameterGroupManifest& manifest() const;

    virtual nx::utils::SharedGuardPtr monitor(
        const Resource& resource, DataBase::Access access) const = 0;

private:
    const api::metrics::ParameterGroupManifest m_manifest;
};

template<typename Resource>
class SingleParameterProvider: public AbstractParameterProvider<Resource>
{
public:
    using Getter = nx::utils::MoveOnlyFunc<Value(const Resource&)>;
    using OnChange = nx::utils::MoveOnlyFunc<void()>;
    using Watch = nx::utils::MoveOnlyFunc<nx::utils::SharedGuardPtr(const Resource&, OnChange)>;

    SingleParameterProvider(
        api::metrics::ParameterManifest manifest, Getter getter, Watch watch = nullptr);

    nx::utils::SharedGuardPtr monitor(
        const Resource& resource, DataBase::Access access) const override;

private:
    const Getter m_getter;
    const Watch m_watch;
};

template<typename Resource>
using ResourceParameterProviders
    = std::vector<std::unique_ptr<AbstractParameterProvider<Resource>>>;

template<typename Resource>
class ParameterGroupProvider: public AbstractParameterProvider<Resource>
{
public:
    ParameterGroupProvider(
        api::metrics::ParameterManifest manifest,
        ResourceParameterProviders<Resource> providers);

    nx::utils::SharedGuardPtr monitor(
        const Resource& resource, DataBase::Access access) const override;

private:
    static api::metrics::ParameterGroupManifest makeManifest(
        api::metrics::ParameterManifest manifest,
        const ResourceParameterProviders<Resource>& providers);

private:
    const ResourceParameterProviders<Resource> m_providers;
};

// -----------------------------------------------------------------------------------------------

template<typename Resource>
AbstractParameterProvider<Resource>::AbstractParameterProvider(
    api::metrics::ParameterGroupManifest manifest)
:
    m_manifest(std::move(manifest))
{
}

template<typename Resource>
const api::metrics::ParameterGroupManifest& AbstractParameterProvider<Resource>::manifest() const
{
    return m_manifest;
}

template<typename Resource>
SingleParameterProvider<Resource>::SingleParameterProvider(
    api::metrics::ParameterManifest manifest, Getter getter, Watch watch)
:
    AbstractParameterProvider<Resource>(api::metrics::makeParameterManifest(
        std::move(manifest.id), std::move(manifest.name), std::move(manifest.unit))),
    m_getter(std::move(getter)),
    m_watch(std::move(watch))
{
}

template<typename Resource>
nx::utils::SharedGuardPtr SingleParameterProvider<Resource>::monitor(
    const Resource& resource, DataBase::Access access) const
{
    const auto update = [this, access, resource]() { access->update(m_getter(resource)); };
    update();

    if (m_watch)
        return m_watch(resource, update);
    else
        return nx::utils::SharedGuardPtr();
}

// -----------------------------------------------------------------------------------------------

template<typename Resource>
ParameterGroupProvider<Resource>::ParameterGroupProvider(
    api::metrics::ParameterManifest manifest,
    ResourceParameterProviders<Resource> providers)
:
    AbstractParameterProvider<Resource>(makeManifest(std::move(manifest), providers)),
    m_providers(std::move(providers))
{
}

template<typename Resource>
nx::utils::SharedGuardPtr ParameterGroupProvider<Resource>::monitor(
    const Resource& resource, DataBase::Access access) const
{
    std::vector<nx::utils::MoveOnlyFunc<void()>> callbacks;
    for (const auto& provider: m_providers)
    {
        const auto parameterAccess = access[provider->manifest().id];
        if (auto guard = provider->monitor(resource, parameterAccess))
            callbacks.emplace_back(*guard->eject());
    }

    return nx::utils::makeSharedGuard(
        [callbacks = std::move(callbacks)] { for (const auto& c: callbacks) c(); });
}

template<typename Resource>
api::metrics::ParameterGroupManifest ParameterGroupProvider<Resource>::makeManifest(
    api::metrics::ParameterManifest manifest,
    const ResourceParameterProviders<Resource>& providers)
{
    auto groupManifest = api::metrics::makeParameterGroupManifest(
        std::move(manifest.id), std::move(manifest.name));

    for (const auto& provider: providers)
        groupManifest.group.push_back(provider->manifest());

    return groupManifest;
}

} // namespace nx::vms::server::metrics
