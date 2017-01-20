#include "business_logic_composite.h"

namespace nx {
namespace hpm {

BusinessLogicComposite::BusinessLogicComposite(
    const conf::Settings& settings,
    nx::stun::MessageDispatcher* stunMessageDispatcher)
:
    m_statsManager(settings),
    m_cloudDataProvider(
        settings.cloudDB().runWithCloud
        ? AbstractCloudDataProviderFactory::create(
            settings.cloudDB().url,
            settings.cloudDB().user.toStdString(),
            settings.cloudDB().password.toStdString(),
            settings.cloudDB().updateInterval)
        : nullptr),
    m_mediaserverApi(m_cloudDataProvider.get(), stunMessageDispatcher),
    m_listeningPeerRegistrator(
        settings,
        m_cloudDataProvider.get(),
        stunMessageDispatcher,
        &m_listeningPeerPool),
    m_cloudConnectProcessor(
        settings,
        m_cloudDataProvider.get(),
        stunMessageDispatcher,
        &m_listeningPeerPool,
        &m_statsManager.collector())
{
    if (!m_cloudDataProvider)
    {
        NX_LOGX(lit("STUN Server is running without cloud (debug mode)"), cl_logALWAYS);
    }
}

PeerRegistrator& BusinessLogicComposite::listeningPeerRegistrator()
{
    return m_listeningPeerRegistrator;
}

ListeningPeerPool& BusinessLogicComposite::listeningPeerPool()
{
    return m_listeningPeerPool;
}

void BusinessLogicComposite::stop()
{
    m_cloudConnectProcessor.stop();
}

} // namespace hpm
} // namespace nx
