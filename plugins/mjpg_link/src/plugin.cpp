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
        return new HttpLinkPlugin();
    }
}

static HttpLinkPlugin* httpLinkPluginInstance = NULL;

HttpLinkPlugin::HttpLinkPlugin()
:
    m_refManager( this ),
    m_pluginContainer( nullptr )
{
    httpLinkPluginInstance = this;
}

HttpLinkPlugin::~HttpLinkPlugin()
{
    httpLinkPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* HttpLinkPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        if (!m_discoveryManager)
            m_discoveryManager.reset(new DiscoveryManager(
                &m_refManager,
                m_pluginContainer));
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

unsigned int HttpLinkPlugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int HttpLinkPlugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* HttpLinkPlugin::name() const
{
    return "mjpg_link";
}

void HttpLinkPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

void HttpLinkPlugin::setPluginContainer(nxpl::PluginContainer* pluginContainer)
{
    m_pluginContainer = pluginContainer;
}

nxpt::CommonRefManager* HttpLinkPlugin::refManager()
{
    return &m_refManager;
}

HttpLinkPlugin* HttpLinkPlugin::instance()
{
    return httpLinkPluginInstance;
}
