/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "plugin.h"
#include "discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new WebCamPlugin();
    }
}

static WebCamPlugin* webCameraPluginInstance = NULL;

WebCamPlugin::WebCamPlugin()
:
    m_refManager( this ),
    m_timeProvider(nullptr)
{
    webCameraPluginInstance = this;
}

WebCamPlugin::~WebCamPlugin()
{
    webCameraPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* WebCamPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int WebCamPlugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int WebCamPlugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* WebCamPlugin::name() const
{
    return "webcam_plugin";
}

void WebCamPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

void WebCamPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    m_timeProvider = static_cast<nxpl::TimeProvider *>(
            pluginContainer->queryInterface(nxpl::IID_TimeProvider));
}

nxpt::CommonRefManager* WebCamPlugin::refManager()
{
    return &m_refManager;
}

WebCamPlugin* WebCamPlugin::instance()
{
    return webCameraPluginInstance;
}
