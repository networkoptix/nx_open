/**********************************************************
* 25 nov 2013
* rvasilenko
***********************************************************/

#ifndef ISD_NATIVE_PLUGIN_H
#define ISD_NATIVE_PLUGIN_H

#include <plugins/plugin_tools.h>
#include <memory>
#include <plugins/plugin_api.h>


/*! 
    This plugin provide access to ISD video stream, motion e.t.c via native ARM SDK. MUST be run on a camera board\n
*/

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class IsdNativePlugin
:
    public nxpl::PluginInterface
{
public:
    IsdNativePlugin();
    virtual ~IsdNativePlugin();

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

    static IsdNativePlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::auto_ptr<DiscoveryManager> m_discoveryManager;
};

#endif  //ISD_NATIVE_PLUGIN_H
