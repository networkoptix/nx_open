// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"

namespace nx::analytics::taxonomy {

Plugin::Plugin(
    nx::vms::api::analytics::PluginDescriptor pluginDescriptor,
    QObject* parent)
    :
    AbstractPlugin(parent),
    m_descriptor(std::move(pluginDescriptor))
{
}

QString Plugin::id() const
{
    return m_descriptor.id;
}

QString Plugin::name() const
{
    return m_descriptor.name;
}

nx::vms::api::analytics::PluginDescriptor Plugin::serialize() const
{
    return m_descriptor;
}

} // namespace nx::analytics::taxonomy
