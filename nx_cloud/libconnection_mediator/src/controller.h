#pragma once

#include "cloud_data_provider.h"
#include "listening_peer_pool.h"
#include "peer_registrator.h"
#include "mediaserver_api.h"
#include "server/hole_punching_processor.h"
#include "statistics/stats_manager.h"

namespace nx {

namespace stun { class MessageDispatcher; }

namespace hpm {

namespace conf { class Settings; }

class Controller
{
public:
    Controller(
        const conf::Settings& settings,
        nx::stun::MessageDispatcher* stunMessageDispatcher);

    PeerRegistrator& listeningPeerRegistrator();
    ListeningPeerPool& listeningPeerPool();

    void stop();

private:
    stats::StatsManager m_statsManager;
    std::unique_ptr<AbstractCloudDataProvider> m_cloudDataProvider;
    MediaserverApi m_mediaserverApi;
    ListeningPeerPool m_listeningPeerPool;
    PeerRegistrator m_listeningPeerRegistrator;
    HolePunchingProcessor m_cloudConnectProcessor;
};

} // namespace hpm
} // namespace nx
