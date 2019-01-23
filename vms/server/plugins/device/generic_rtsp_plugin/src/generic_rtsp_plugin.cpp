/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_plugin.h"

#include <memory.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_api.h>

#include "generic_rtsp_discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new GenericRTSPPlugin();
    }
}

static GenericRTSPPlugin* genericRTSPPluginInstance = NULL;

GenericRTSPPlugin::GenericRTSPPlugin()
:
    m_refManager( this )
{
    genericRTSPPluginInstance = this;

    m_discoveryManager.reset( new GenericRTSPDiscoveryManager() );
}

GenericRTSPPlugin::~GenericRTSPPlugin()
{
    genericRTSPPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* GenericRTSPPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
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
    if( memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof( nxpl::IID_Plugin) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    return NULL;
}

int GenericRTSPPlugin::addRef() const
{
    return m_refManager.addRef();
}

int GenericRTSPPlugin::releaseRef() const
{
    return m_refManager.releaseRef();
}

const char* GenericRTSPPlugin::name() const
{
    return "GENERIC_RTSP";
}

void GenericRTSPPlugin::setSettings( const nxpl::Setting* /*settings*/, int /*count*/ )
{
}

nxpt::CommonRefManager* GenericRTSPPlugin::refManager()
{
    return &m_refManager;
}

GenericRTSPPlugin* GenericRTSPPlugin::instance()
{
    return genericRTSPPluginInstance;
}
