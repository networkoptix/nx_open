#pragma once

#include <nx/sdk/ptr.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>

namespace nx {
namespace usb_cam {

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class Plugin: public nxpl::Plugin2
{
public:
    Plugin();
    virtual ~Plugin();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

    virtual const char* name() const override;
    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    nxpt::CommonRefManager* refManager();
    static Plugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    nx::sdk::Ptr<DiscoveryManager> m_discoveryManager;
    nx::sdk::Ptr<nxpl::TimeProvider> m_timeProvider;
};

} // namespace nx
} // namespace usb_cam
