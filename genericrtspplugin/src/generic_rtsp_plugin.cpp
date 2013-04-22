/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_plugin.h"

#include <plugins/camera_plugin.h>
#include <plugins/nx_plugin_api.h>

#include "generic_rtsp_discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::NXPluginInterface* createNXPluginInstance()
    {
        return new GenericRTSPPlugin();
    }
}

static GenericRTSPPlugin* genericRTSPPluginInstance = NULL;

GenericRTSPPlugin::GenericRTSPPlugin()
{
    genericRTSPPluginInstance = this;

    m_discoveryManager.reset( new GenericRTSPDiscoveryManager() );
}

GenericRTSPPlugin::~GenericRTSPPlugin()
{
    genericRTSPPluginInstance = NULL;
}

//!Implementation of nxpl::NXPluginInterface::queryInterface
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

    return NULL;
}

GenericRTSPPlugin* GenericRTSPPlugin::instance()
{
    return genericRTSPPluginInstance;
}
