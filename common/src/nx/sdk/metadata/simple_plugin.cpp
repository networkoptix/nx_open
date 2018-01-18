#include "simple_plugin.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

// TODO: #mike: Decide on NX_PRINT_PREFIX to reflect actual plugin class.

SimplePlugin::SimplePlugin(const char* name):
    m_name(name)
{
    NX_PRINT << "Created \"" << m_name << "\"";
}

SimplePlugin::~SimplePlugin()
{
    NX_PRINT << "Destroyed \"" << m_name << "\"";
}

void* SimplePlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataPlugin)
    {
        addRef();
        return static_cast<AbstractMetadataPlugin*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* SimplePlugin::name() const
{
    return m_name;
}

void SimplePlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void SimplePlugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void SimplePlugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
}

AbstractSerializer* SimplePlugin::serializerForType(
    const nxpl::NX_GUID& /*typeGuid*/,
    Error* /*outError*/)
{
    return nullptr;
}

} // namespace metadata
} // namespace sdk
} // namespace nx
