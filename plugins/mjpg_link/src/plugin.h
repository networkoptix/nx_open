/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef IMAGE_LIBRARY_PLUGIN_H
#define IMAGE_LIBRARY_PLUGIN_H

#include <plugins/plugin_tools.h>

#include <memory>

#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>

struct StreamStateCache
{
    bool isRunning = false;
    nx::utils::ElapsedTimer timer;
};

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

    static HttpLinkPlugin* instance();

    void setStreamState(const QUrl& url, bool isStreamRunning);

    bool isStreamRunning(const QUrl& url) const;

private:
    mutable QnMutex m_mutex;
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<DiscoveryManager> m_discoveryManager;
    nxpl::TimeProvider *m_timeProvider;
    std::map<QUrl, StreamStateCache> m_streamStateCache;
};

#endif  //IMAGE_LIBRARY_PLUGIN_H
