/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "axis_camera_plugin.h"

#include <plugins/camera_plugin.h>
#include <plugins/nx_plugin_api.h>

#include "axis_discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::NXPluginInterface* createNXPluginInstance()
    {
        return new AxisCameraPlugin();
    }
}

static AxisCameraPlugin* axisCameraPluginInstance = NULL;

AxisCameraPlugin::AxisCameraPlugin()
:
    m_discoveryManager( new AxisCameraDiscoveryManager() )
{
    axisCameraPluginInstance = this;

    m_networkEventLoopThread.setObjectName( "AxisCameraPlugin" );
    m_networkEventLoopThread.start();
    m_networkAccessManager.moveToThread( &m_networkEventLoopThread );
}

AxisCameraPlugin::~AxisCameraPlugin()
{
    m_networkAccessManager.deleteLater();

    m_networkEventLoopThread.quit();
    m_networkEventLoopThread.wait();

    m_discoveryManager->releaseRef();

    axisCameraPluginInstance = NULL;
}

//!Implementation of nxpl::NXPluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* AxisCameraPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        m_discoveryManager->addRef();
        return m_discoveryManager;
    }

    return NULL;
}

QNetworkAccessManager* AxisCameraPlugin::networkAccessManager()
{
    return &m_networkAccessManager;
}

AxisCameraPlugin* AxisCameraPlugin::instance()
{
    return axisCameraPluginInstance;
}
