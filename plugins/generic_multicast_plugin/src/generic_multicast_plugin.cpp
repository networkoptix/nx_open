#include <memory.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_api.h>

#include "generic_multicast_discovery_manager.h"
#include "generic_multicast_plugin.h"

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface* createNXPluginInstance()
    {
        return new GenericMulticastPlugin();
    }
}

static GenericMulticastPlugin* genericMulticastPluginInstance = nullptr;

GenericMulticastPlugin::GenericMulticastPlugin()
:
    m_refManager( this )
{
    genericMulticastPluginInstance = this;
    m_discoveryManager.reset(new GenericMulticastDiscoveryManager());
}

GenericMulticastPlugin::~GenericMulticastPlugin()
{
    genericMulticastPluginInstance = NULL;
}

void* GenericMulticastPlugin::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager)) == 0)
    {
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if (memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof(nxpl::IID_Plugin) ) == 0)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    return nullptr;
}

unsigned int GenericMulticastPlugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastPlugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* GenericMulticastPlugin::name() const
{
    return "GENERIC_MULTICAST";
}

void GenericMulticastPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

nxpt::CommonRefManager* GenericMulticastPlugin::refManager()
{
    return &m_refManager;
}

GenericMulticastPlugin* GenericMulticastPlugin::instance()
{
    return genericMulticastPluginInstance;
}
