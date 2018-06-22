#include "plugin.h"
extern "C" {
#include <libavdevice/avdevice.h>
} // extern "C"

#include "discovery_manager.h"

extern "C" {
nxpl::PluginInterface* createNXPluginInstance()
{
    return new nx::webcam_plugin::Plugin();
}
} // extern "C"

namespace nx {
namespace webcam_plugin {

static Plugin* webCameraPluginInstance = NULL;

Plugin::Plugin()
:
    m_refManager( this ),
    m_timeProvider(nullptr)
{
    avdevice_register_all();
    webCameraPluginInstance = this;
}

Plugin::~Plugin()
{
    webCameraPluginInstance = NULL;
}

void* Plugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        if (!m_discoveryManager)
            m_discoveryManager.reset(new DiscoveryManager(
                &m_refManager,
                m_timeProvider));
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof( nxpl::IID_Plugin) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_Plugin2, sizeof( nxpl::IID_Plugin2) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    return NULL;
}

unsigned int Plugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int Plugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* Plugin::name() const
{
    return "webcam_plugin";
}

void Plugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

void Plugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    m_timeProvider = static_cast<nxpl::TimeProvider *>(
            pluginContainer->queryInterface(nxpl::IID_TimeProvider));
}

nxpt::CommonRefManager* Plugin::refManager()
{
    return &m_refManager;
}

Plugin* Plugin::instance()
{
    return webCameraPluginInstance;
}

} // namespace nx
} // namespace webcam_plugin