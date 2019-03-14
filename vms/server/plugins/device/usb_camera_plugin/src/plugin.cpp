#include "plugin.h"
extern "C" {
#include <libavdevice/avdevice.h>
} // extern "C"

#include "discovery_manager.h"

extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
nxpl::PluginInterface* createNXPluginInstance()
{
    return new nx::usb_cam::Plugin();
}
} // extern "C"

namespace nx {
namespace usb_cam {

namespace {

static constexpr const char kPluginName[] = "usb_cam";

}

static Plugin* webCameraPluginInstance = NULL;

Plugin::Plugin():
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
    if (memcmp(
        &interfaceID,
        &nxcip::IID_CameraDiscoveryManager,
        sizeof(nxcip::IID_CameraDiscoveryManager)) == 0 )
    {
        if (!m_discoveryManager)
            m_discoveryManager.reset(new DiscoveryManager(
                &m_refManager,
                m_timeProvider));
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if ( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if ( memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof( nxpl::IID_Plugin) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }
    if ( memcmp( &interfaceID, &nxpl::IID_Plugin2, sizeof( nxpl::IID_Plugin2) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    return NULL;
}

int Plugin::addRef() const
{
    return m_refManager.addRef();
}

int Plugin::releaseRef() const
{
    return m_refManager.releaseRef();
}

const char* Plugin::name() const
{
    return kPluginName;
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
} // namespace usb_cam
