/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#ifndef GENERIC_RTSP_PLUGIN_H
#define GENERIC_RTSP_PLUGIN_H

#include "common_ref_manager.h"

#include <memory>

#include <plugins/plugin_api.h>


/*! \main
    Object life-time management:\n
    - all classes, implementing \a nxcip interfaces, delegate reference counting (by using \a CommonRefManager(CommonRefManager*) constructor) 
        to factory class instance (e.g., \a AxisCameraManager is a factory for \a AxisRelayIOManager, \a AxisMediaEncoder, etc.)
    - all factory classes (except for \a AxisCameraDiscoveryManager) hold pointer to child class object (e.g., \a AxisRelayIOManager is a child for \a AxisCameraManager)
        and delete all children on destruction
*/

class GenericRTSPDiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class GenericRTSPPlugin
:
    public nxpl::PluginInterface
{
public:
    GenericRTSPPlugin();
    virtual ~GenericRTSPPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    CommonRefManager* refManager();

    static GenericRTSPPlugin* instance();

private:
    CommonRefManager m_refManager;
    std::auto_ptr<GenericRTSPDiscoveryManager> m_discoveryManager;
};

#endif  //GENERIC_RTSP_PLUGIN_H
