#include "relay_cluster_client.h"

namespace nx {
namespace hpm {

RelayClusterClient::RelayClusterClient(const conf::Settings& settings):
    m_settings(settings)
{
}

RelayClusterClient::~RelayClusterClient()
{
    m_aioThreadBinder.pleaseStopSync();
}

void RelayClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& /*peerId*/,
    RelayInstanceSelectCompletionHandler completionHandler)
{
    // TODO: #Nate selecting online relays in same geograpic region instead of from settings.

    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                !m_settings.trafficRelay().urls.empty()
                    ? cloud::relay::api::ResultCode::ok
                    : cloud::relay::api::ResultCode::notFound,
                m_settings.trafficRelay().urls);
        });
}

void RelayClusterClient::findRelayInstancePeerIsListeningOn(
    const std::string& /*peerId*/,
    RelayInstanceSearchCompletionHandler completionHandler)
{
    // TODO: #Nate find relay instance that this peer is listening on from online relays db?

    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            QUrl relayInstanceUrl;
            if (!m_settings.trafficRelay().urls.empty())
                relayInstanceUrl = m_settings.trafficRelay().urls.front();

            completionHandler(
                !m_settings.trafficRelay().urls.empty()
                    ? cloud::relay::api::ResultCode::ok
                    : cloud::relay::api::ResultCode::notFound,
                relayInstanceUrl);
        });
}

} // namespace hpm
} // namespace nx
