#pragma once

#include <memory>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_api.h>

class GenericMulticastDiscoveryManager;

class GenericMulticastPlugin
:
    public nxpl::Plugin
{
public:
    GenericMulticastPlugin();
    virtual ~GenericMulticastPlugin();

    /** Implementation of nxpl::PluginInterface::queryInterface */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    /** Implementation of nxpl::PluginInterface::addRef */
    virtual unsigned int addRef() override;
    /** Implementation of nxpl::PluginInterface::releaseRef */
    virtual unsigned int releaseRef() override;

    /** Implementation of nxpl::Plugin::name */
    virtual const char* name() const override;
    /** Implementation of nxpl::Plugin::setSettings */
    virtual void setSettings( const nxpl::Setting* settings, int count ) override;

    nxpt::CommonRefManager* refManager();

    static GenericMulticastPlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<GenericMulticastDiscoveryManager> m_discoveryManager;
};
