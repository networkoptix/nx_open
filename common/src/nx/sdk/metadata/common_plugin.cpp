#include "common_plugin.h"

#define NX_PRINT_PREFIX (std::string("[") + m_name + "] ")
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

CommonPlugin::CommonPlugin(const char* name):
    m_name(name)
{
    NX_PRINT << "Created " << this;
}

std::string CommonPlugin::getParamValue(const char* paramName)
{
    return m_settings[paramName];
}

CommonPlugin::~CommonPlugin()
{
    NX_PRINT << "Destroyed " << this;
}

void* CommonPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Plugin)
    {
        addRef();
        return static_cast<Plugin*>(this);
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

const char* CommonPlugin::name() const
{
    return m_name;
}

void CommonPlugin::setSettings(const nxpl::Setting* settings, int count)
{
    for (int i = 0; i < count; ++i)
        m_settings[settings[i].name] = settings[i].value;
}

void CommonPlugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void CommonPlugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
}

const char* CommonPlugin::capabilitiesManifest(Error* error) const
{
    m_manifest = capabilitiesManifest();
    return m_manifest.c_str();
}

} // namespace metadata
} // namespace sdk
} // namespace nx
