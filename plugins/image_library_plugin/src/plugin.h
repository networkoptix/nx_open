/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef IMAGE_LIBRARY_PLUGIN_H
#define IMAGE_LIBRARY_PLUGIN_H

#include <plugins/plugin_tools.h>

#include <memory>

#include <plugins/plugin_api.h>


/*! \mainpage
    \par
    This project demonstrates usage of Network Optix camera integration plugin API to add support for camera with remote archive (e.g., storage is bound directly to camera)\n

    \par Build how-to
    Compiles to dynamic library. Tested on MS Windows 7 and Ubuntu 12.04.\n
    Does not require any third party library\n

    To build on linux:
    \code
    CD %SDK_DIR%/sample/image_library_plugin
    make
    \endcode
    On windows in directory %SDK_DIR%/sample/image_library_plugin/win You can find image_library_plugin.vcproj project file for msvc2008.
    Successful build results in libimage_library_plugin.so (linux) and image_library_plugin.dll (mswin)

    \par Usage
        You MUST have HD Witness mediaserver installed to use this plugin.\n
    To use plugin simply put built library to mediaserver directory (by default, "C:\Program Files\Network Optix\HD Witness\Mediaserver" on ms windows and
    /opt/networkoptix/mediaserver/bin/ on linux) and restart server.\n
        To use this plugin, launch client aplication and use manual camera search ("Add Camera(s)..." in media server menu) filling "Camera Address" field with 
    absolute path to local directory, containing jpeg file(s). Specified directory will be found as camera with archive and appear in tree menu.

    \par Implements following camera integration interfaces:
    - \a nxcip::CameraDiscoveryManager to enable AXIS camera discovery by MDNS
    - \a nxcip::BaseCameraManager2 to retrieve camera properties and access other managers
    - \a nxcip::CameraMediaEncoder2 to receive LIVE media stream from camera
    - \a nxcip::StreamReader to provide media stream directly from plugin
    - \a nxcip::DtsArchiveReader to access camera's archive

    \par Object life-time management:
    - all classes, implementing \a nxcip interfaces, delegate reference counting (by using \a CommonRefManager(CommonRefManager*) constructor) 
        to factory class instance (e.g., \a AxisCameraManager is a factory for \a AxisRelayIOManager, \a AxisMediaEncoder, etc.)
    - all factory classes (except for \a AxisCameraDiscoveryManager) hold pointer to child class object (e.g., \a AxisRelayIOManager is a child for \a AxisCameraManager)
        and delete all children on destruction
*/

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class ImageLibraryPlugin
:
    public nxpl::PluginInterface
{
public:
    ImageLibraryPlugin();
    virtual ~ImageLibraryPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    nxpt::CommonRefManager* refManager();

    static ImageLibraryPlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::auto_ptr<DiscoveryManager> m_discoveryManager;
};

#endif  //IMAGE_LIBRARY_PLUGIN_H
