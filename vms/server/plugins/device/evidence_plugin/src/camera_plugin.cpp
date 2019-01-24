/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include "camera_plugin.h"

#include <camera/camera_plugin.h>
#include <plugins/plugin_api.h>

#include "discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new CameraPlugin();
    }
}

static CameraPlugin* axisCameraPluginInstance = NULL;

CameraPlugin::CameraPlugin()
:
    m_refManager( this )
{
    axisCameraPluginInstance = this;

    m_discoveryManager.reset( new CameraDiscoveryManager() );

    m_networkEventLoopThread.setObjectName( "E-Vidence plugin NetworkAccessManager thread" );
    m_networkEventLoopThread.start();
    m_networkAccessManager.moveToThread( &m_networkEventLoopThread );

    m_qtEventLoopThread.setObjectName( "E-Vidence plugin internal thread" );
    m_qtEventLoopThread.start();
}

CameraPlugin::~CameraPlugin()
{
    m_networkAccessManager.deleteLater();

    m_qtEventLoopThread.quit();
    m_qtEventLoopThread.wait();

    m_networkEventLoopThread.quit();
    m_networkEventLoopThread.wait();

    axisCameraPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* CameraPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
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

int CameraPlugin::addRef() const
{
    return m_refManager.addRef();
}

int CameraPlugin::releaseRef() const
{
    return m_refManager.releaseRef();
}

nxpt::CommonRefManager* CameraPlugin::refManager()
{
    return &m_refManager;
}

QNetworkAccessManager* CameraPlugin::networkAccessManager()
{
    return &m_networkAccessManager;
}

QThread* CameraPlugin::qtEventLoopThread()
{
    return &m_qtEventLoopThread;
}

CameraPlugin* CameraPlugin::instance()
{
    return axisCameraPluginInstance;
}
