/**********************************************************
* 17 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_CAMERA_PLUGIN_H
#define AXIS_CAMERA_PLUGIN_H

#include <plugins/plugin_tools.h>

#include <memory>

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QThread>

#include <plugins/plugin_api.h>

/*! \mainpage
    \par
    This project demonstrates usage of camera integration plugin API to add AXIS camera support to mediaserver. Overrides built-in AXIS camera driver.\n

    \par Build how-to
    Use provided CMakeLists.txt project file to generate solution for your favorite build tool or IDE.
    It is Qt 5 project. Compiles to dynamic library. Tested on MS Windows 10 and Ubuntu 16.04.LTS\n
    To build You MUST have Qt 5.0+ installed.\n
    \warning To build plugin using cmake on Ubuntu you need to install qtbase5-dev:\n
    $sudo apt-get install qtbase5-dev
    \warning Qt is used here to simplify this sample only! It is not required to use Qt in real production plugin. Provided API uses c++ only (even STL is not required)

    To build:
    \code
    CD %SDK_DIR%/sample/axis_camera_plugin
    qmake
    nmake release (on ms windows)
    make release (on linux)
    \endcode
    On successful build You will find libaxis_camera_plugin.so (linux) or axis_camera_plugin.dll (mswin) in %SDK_DIR%/sample/axis_camera_plugin/release directory

    \par Usage
    You MUST have mediaserver installed to use this plugin.\n
    To use plugin simply put built library to mediaserver directory and restart server

    \par Implements following camera integration interfaces:
    - \a nxcip::CameraDiscoveryManager to enable AXIS camera discovery by MDNS
    - \a nxcip::BaseCameraManager to retrieve camera properties and access other managers
    - \a nxcip::CameraMediaEncoder to receive media stream from camera
    - \a nxcip::CameraRelayIOManager to receive relay input port change state events and change relay output port state

    \par Object life-time management:
    - all classes, implementing \a nxcip interfaces, delegate reference counting (by using \a CommonRefManager(CommonRefManager*) constructor)
        to factory class instance (e.g., \a AxisCameraManager is a factory for \a AxisRelayIOManager, \a AxisMediaEncoder, etc.)
    - all factory classes (except for \a AxisCameraDiscoveryManager) hold pointer to child class object (e.g., \a AxisRelayIOManager is a child for \a AxisCameraManager)
        and delete all children on destruction
*/

class AxisCameraDiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class AxisCameraPlugin
:
    public nxpl::PluginInterface
{
public:
    AxisCameraPlugin();
    virtual ~AxisCameraPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    nxpt::CommonRefManager* refManager();
    QNetworkAccessManager* networkAccessManager();

    static AxisCameraPlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<AxisCameraDiscoveryManager> m_discoveryManager;
    //!Used with QNetworkAccessManager
    QThread m_networkEventLoopThread;
    QNetworkAccessManager m_networkAccessManager;
};

#endif  //AXIS_CAMERA_PLUGIN_H
