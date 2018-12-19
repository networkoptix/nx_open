#pragma once

#include <plugins/plugin_tools.h>

#include <memory>

#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

namespace nx::vms_server_plugins::mjpeg_link {

class DiscoveryManager;

//!Main plugin class. Hosts and initializes necessary internal data
class HttpLinkPlugin
:
    public nxpl::Plugin2
{
public:
    HttpLinkPlugin();
    virtual ~HttpLinkPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    //!Implementation of nxpl::Plugin::name
    virtual const char* name() const override;
    //!Implementation of nxpl::Plugin::setSettings
    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    //!Implementation of nxpl::Plugin2::setPluginContainer
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    nxpt::CommonRefManager* refManager();

    static HttpLinkPlugin* instance();

    void setStreamState(const nx::utils::Url& url, bool isStreamRunning);

    bool isStreamRunning(const nx::utils::Url& url) const;

private:
    void cleanStreamCacheUp();

private:
    mutable QnMutex m_mutex;
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<DiscoveryManager> m_discoveryManager;
    nxpl::TimeProvider *m_timeProvider;

    nx::utils::ElapsedTimer m_streamStateCacheCleanupTimer;
    std::map<nx::utils::Url, nx::utils::ElapsedTimer> m_streamStateCache;
};

} // namespace nx::vms_server_plugins::mjpeg_link
