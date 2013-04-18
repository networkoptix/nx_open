/**********************************************************
* 17 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_CAMERA_PLUGIN_H
#define AXIS_CAMERA_PLUGIN_H

#include "common_ref_manager.h"

#include <memory>

#include <QNetworkAccessManager>
#include <QThread>


/*! \main
    - all classes, implementing \a nxcip interfaces, hold reference to factory class instance 
        (e.g., \a AxisCameraManager is a factory for \a AxisRelayIOManager, \a AxisMediaEncoder, etc.)
    - all factory classes (except for \a AxisCameraDiscoveryManager) hold weak reference to child class object (e.g., \a AxisRelayIOManager is a child for \a AxisCameraManager)
    - if factory have to hold reference to some instance, it uses weak reference to avoid reference loop.
        E.g., \a AxisCameraManager holds weak reference to \a AxisRelayIOManager
    - all classes, implementing \a nxcip interfaces, hold reference to \a AxisCameraPlugin instance
*/


class AxisCameraDiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class AxisCameraPlugin
:
    public CommonRefManager
{
public:
    AxisCameraPlugin();
    virtual ~AxisCameraPlugin();

    //!Implementation of nxpl::NXPluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

    QNetworkAccessManager* networkAccessManager();

    static AxisCameraPlugin* instance();

private:
    //!TODO/IMPL this MUST be weak pointer
    AxisCameraDiscoveryManager* m_discoveryManager;
    //!Used with QNetworkAccessManager
    QThread m_networkEventLoopThread;
    QNetworkAccessManager m_networkAccessManager;
};

#endif  //AXIS_CAMERA_PLUGIN_H
