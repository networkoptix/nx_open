#include "controller.h"

namespace nx {
namespace hpm {

Controller::Controller(
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

PeerRegistrator& Controller::listeningPeerRegistrator()
{
    return m_listeningPeerRegistrator;
}

ListeningPeerPool& Controller::listeningPeerPool()
{
    return m_listeningPeerPool;
}

void Controller::stop()
{
    m_cloudConnectProcessor.stop();
}

} // namespace hpm
} // namespace nx
