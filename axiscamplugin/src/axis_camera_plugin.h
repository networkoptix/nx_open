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

#include <plugins/nx_plugin_api.h>


/*! \main
    Object life-time management:\n
    - all classes, implementing \a nxcip interfaces, delegate reference counting (by using \a CommonRefManager(CommonRefManager*) constructor) 
        to factory class instance (e.g., \a AxisCameraManager is a factory for \a AxisRelayIOManager, \a AxisMediaEncoder, etc.)
    - all factory classes (except for \a AxisCameraDiscoveryManager) hold pointer to child class object (e.g., \a AxisRelayIOManager is a child for \a AxisCameraManager)
        and delete all children on destruction
*/

class AxisCameraDiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class AxisCameraPlugin
:
    public nxpl::NXPluginInterface,
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
    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::NXPluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    QNetworkAccessManager* networkAccessManager();

    static AxisCameraPlugin* instance();

private:
    std::auto_ptr<AxisCameraDiscoveryManager> m_discoveryManager;
    //!Used with QNetworkAccessManager
    QThread m_networkEventLoopThread;
    QNetworkAccessManager m_networkAccessManager;
};

#endif  //AXIS_CAMERA_PLUGIN_H
