/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "axis_camera_plugin.h"

#include <camera/camera_plugin.h>
#include <plugins/plugin_api.h>

#include "axis_discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new AxisCameraPlugin();
    }
}

static AxisCameraPlugin* axisCameraPluginInstance = NULL;

AxisCameraPlugin::AxisCameraPlugin()
:
    m_refManager( this )
{
    axisCameraPluginInstance = this;

    m_discoveryManager.reset( new AxisCameraDiscoveryManager() );

    m_networkEventLoopThread.setObjectName( "AxisCameraPlugin" );
    m_networkEventLoopThread.start();
    m_networkAccessManager.moveToThread( &m_networkEventLoopThread );
}

AxisCameraPlugin::~AxisCameraPlugin()
{
    m_networkAccessManager.deleteLater();

    m_networkEventLoopThread.quit();
    m_networkEventLoopThread.wait();

    axisCameraPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* AxisCameraPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
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

int AxisCameraPlugin::addRef() const
{
    return m_refManager.addRef();
}

int AxisCameraPlugin::releaseRef() const
{
    return m_refManager.releaseRef();
}

nxpt::CommonRefManager* AxisCameraPlugin::refManager()
{
    return &m_refManager;
}

QNetworkAccessManager* AxisCameraPlugin::networkAccessManager()
{
    return &m_networkAccessManager;
}

AxisCameraPlugin* AxisCameraPlugin::instance()
{
    return axisCameraPluginInstance;
}
