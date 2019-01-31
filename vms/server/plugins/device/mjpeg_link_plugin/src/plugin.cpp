#include "plugin.h"

#include <chrono>

#include "discovery_manager.h"

#include <nx/utils/log/log.h>

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace {

static const milliseconds kStreamStateExpirationPeriod = 5s;
static const milliseconds kStreamStateCacheCleanupPeriod = 5min;

} // namespace

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new nx::vms_server_plugins::mjpeg_link::HttpLinkPlugin();
    }
}

namespace nx::vms_server_plugins::mjpeg_link {

static HttpLinkPlugin* httpLinkPluginInstance = NULL;

HttpLinkPlugin::HttpLinkPlugin():
    m_refManager( this ),
    m_timeProvider(nullptr)
{
    httpLinkPluginInstance = this;
}

HttpLinkPlugin::~HttpLinkPlugin()
{
    httpLinkPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* HttpLinkPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        if (!m_discoveryManager)
            m_discoveryManager.reset(new DiscoveryManager(
                &m_refManager,
                m_timeProvider));
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof( nxpl::IID_Plugin) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_Plugin2, sizeof( nxpl::IID_Plugin2) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    return NULL;
}

unsigned int HttpLinkPlugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int HttpLinkPlugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* HttpLinkPlugin::name() const
{
    return "mjpeg_link_plugin";
}

void HttpLinkPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

void HttpLinkPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    m_timeProvider = static_cast<nxpl::TimeProvider *>(
            pluginContainer->queryInterface(nxpl::IID_TimeProvider));
}

nxpt::CommonRefManager* HttpLinkPlugin::refManager()
{
    return &m_refManager;
}

HttpLinkPlugin* HttpLinkPlugin::instance()
{
    return httpLinkPluginInstance;
}

void HttpLinkPlugin::setStreamState(const nx::utils::Url& url, bool isStreamRunning)
{
    NX_VERBOSE(this, "%1 set stream state = %2", url, isStreamRunning);

    QnMutexLocker lock(&m_mutex);
    if (!m_streamStateCacheCleanupTimer.isValid()
        || m_streamStateCacheCleanupTimer.hasExpired(kStreamStateCacheCleanupPeriod))
    {
        cleanStreamCacheUp();
    }

    auto& timer = m_streamStateCache[url];
    if (isStreamRunning)
        timer.restart();
    else
        timer.invalidate();

}

bool HttpLinkPlugin::isStreamRunning(const nx::utils::Url& url) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_streamStateCache.find(url);
    if (itr == m_streamStateCache.cend())
        return false;

    const auto& timer = itr->second;
    const auto isRunning = timer.isValid() && !timer.hasExpired(kStreamStateExpirationPeriod);
    NX_DEBUG(this, "%1 got stream state = %2", url, isRunning);
    return isRunning;
}

void HttpLinkPlugin::cleanStreamCacheUp()
{
    for (auto itr = m_streamStateCache.cbegin(); itr != m_streamStateCache.cend();)
    {
        auto& timer = itr->second;
        if (!timer.isValid() || timer.hasExpired(kStreamStateExpirationPeriod))
        {
            NX_VERBOSE(this, "%1 state is expired", itr->first);
            itr = m_streamStateCache.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
    m_streamStateCacheCleanupTimer.restart();
}

} // namespace nx::vms_server_plugins::mjpeg_link
