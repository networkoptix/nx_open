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
    m_refManager( this )
{
    httpLinkPluginInstance = this;

    m_discoveryManager.reset( new DiscoveryManager() );
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
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
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

nxpt::CommonRefManager* HttpLinkPlugin::refManager()
{
    return &m_refManager;
}

HttpLinkPlugin* HttpLinkPlugin::instance()
{
    return httpLinkPluginInstance;
}
