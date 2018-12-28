#include "plugin.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

Plugin::Plugin(
    std::string pluginName,
    std::string pluginManifest,
    CreateEngine createEngine)
    :
    m_name(std::move(pluginName)),
    m_manifest(std::move(pluginManifest)),
    m_createEngine(std::move(createEngine))
{
    NX_PRINT << "Created " << m_name << "[" << this << "]";
}

Plugin::~Plugin()
{
    NX_PRINT << "Destroyed " << m_name << "[" << this << "]";
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Plugin)
    {
        addRef();
        return static_cast<IPlugin*>(this);
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

const char* Plugin::name() const
{
    return m_name.c_str();
}

void Plugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Here roSettings are passed from Server. Currently, they are not used by analytics plugins,
    // thus, do nothing.
}

void Plugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    m_pluginContainer = pluginContainer;
}

const IString* Plugin::manifest(nx::sdk::Error* outError) const
{
    if (outError)
        *outError = nx::sdk::Error::noError;

    return new String(m_manifest);
}

IEngine* Plugin::createEngine(Error* outError)
{
    IEngine* engine = m_createEngine(this);
    if (!engine)
    {
        NX_PRINT << "ERROR: " << m_name << ": createEngine() failed";
        *outError = Error::unknownError;
        return nullptr;
    }
    return engine;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
