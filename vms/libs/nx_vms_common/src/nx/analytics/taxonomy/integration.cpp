// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

namespace nx::analytics::taxonomy {

Integration::Integration(
    nx::vms::api::analytics::PluginDescriptor pluginDescriptor,
    QObject* parent)
    :
    AbstractIntegration(parent),
    m_descriptor(std::move(pluginDescriptor))
{
}

QString Integration::id() const
{
    return m_descriptor.id;
}

QString Integration::name() const
{
    return m_descriptor.name;
}

nx::vms::api::analytics::PluginDescriptor Integration::serialize() const
{
    return m_descriptor;
}

} // namespace nx::analytics::taxonomy
