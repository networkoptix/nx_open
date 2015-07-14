/**********************************************************
* 25 nov 2013
* rvasilenko
***********************************************************/

#ifndef ISD_NATIVE_PLUGIN_H
#define ISD_NATIVE_PLUGIN_H

#include <memory>
#include <string>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_api.h>

#include "string_conversions.h"


/*! 
    This plugin provide access to ISD video stream, motion e.t.c via native ARM SDK. MUST be run on a camera board\n
*/

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class IsdNativePlugin
:
    public nxpl::Plugin
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

    //!Implementation of nxpl::Plugin::name
    virtual const char* name() const override;
    //!Implementation of nxpl::Plugin::setSettings
    virtual void setSettings( const nxpl::Setting* settings, size_t count ) override;

    nxpt::CommonRefManager* refManager();

    template<class T> T getSetting( const std::string& name, T defaultValue = T() ) const
    {
        auto valIter = m_settings.find( name );
        if( valIter == m_settings.end() )
            return defaultValue;

        return convert<T>(valIter->second);
    }

    static IsdNativePlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::auto_ptr<DiscoveryManager> m_discoveryManager;
    std::map<std::string, std::string> m_settings;
};

#endif  //ISD_NATIVE_PLUGIN_H
