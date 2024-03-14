// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::utils::metrics {

enum class Scope
{
    local, //< The value is only monitored for local resources.
    site //< The value is monitored for all known resources in the site.
};

NX_VMS_UTILS_API QString toString(Scope scope);

/**
 *  Describes resource for monitoring. May be inherited to hold resource ownership.
 */
struct NX_VMS_UTILS_API ResourceDescription
{
    ResourceDescription(QString id, Scope scope);
    virtual ~ResourceDescription() = default;

    ResourceDescription(const ResourceDescription&) = delete;
    ResourceDescription& operator=(const ResourceDescription&) = delete;

    const QString id;
    const Scope scope = Scope::local;
};

/**
 *  Holds resource ownership and describes resource for monitoring.
 */
template<typename ResourceType>
struct TypedResourceDescription: ResourceDescription
{
    TypedResourceDescription(ResourceType resource, QString id, Scope scope);

    ResourceType resource;
};

// ------------------------------------------------------------------------------------------------

template<typename ResourceType>
TypedResourceDescription<ResourceType>::TypedResourceDescription(
    ResourceType resource, QString id, Scope scope)
:
    ResourceDescription(std::move(id), scope),
    resource(std::move(resource))
{
}

} // namespace nx::vms::utils::metrics
