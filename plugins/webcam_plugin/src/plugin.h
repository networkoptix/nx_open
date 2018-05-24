#ifndef IMAGE_LIBRARY_PLUGIN_H
#define IMAGE_LIBRARY_PLUGIN_H

#include <memory>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>

namespace nx {
namespace webcam_plugin {

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class Plugin
:
    public nxpl::Plugin2
{
public:
    Plugin();
    virtual ~Plugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxpl::Plugin::name
    virtual const char* name() const override;
    //!Implementation of nxpl::Plugin::setSettings
    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    //!Implementation of nxpl::Plugin2::setPluginContainer
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    nxpt::CommonRefManager* refManager();

    static Plugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<DiscoveryManager> m_discoveryManager;
    nxpl::TimeProvider *m_timeProvider;
};

} // namespace nx 
} // namespace webcam_plugin 

#endif  //IMAGE_LIBRARY_PLUGIN_H
