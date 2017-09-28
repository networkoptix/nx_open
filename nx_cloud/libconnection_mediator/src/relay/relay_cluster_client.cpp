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
    RelayInstanceSearchCompletionHandler completionHandler)
{
    // TODO: #ak Selecting any online relay instance.

    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            QUrl relayInstanceUrl;
            if (!m_settings.trafficRelay().url.isEmpty())
                relayInstanceUrl = m_settings.trafficRelay().url;

            completionHandler(
                !relayInstanceUrl.isEmpty()
                    ? cloud::relay::api::ResultCode::ok
                    : cloud::relay::api::ResultCode::notFound,
                relayInstanceUrl);
        });
}

void RelayClusterClient::findRelayInstancePeerIsListeningOn(
    const std::string& /*peerId*/,
    RelayInstanceSearchCompletionHandler completionHandler)
{
    // TODO: #ak Selecting relay instance listening peer can be found on.

    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            QUrl relayInstanceUrl;
            if (!m_settings.trafficRelay().url.isEmpty())
                relayInstanceUrl = m_settings.trafficRelay().url;

            completionHandler(
                !relayInstanceUrl.isEmpty()
                    ? cloud::relay::api::ResultCode::ok
                    : cloud::relay::api::ResultCode::notFound,
                relayInstanceUrl);
        });
}

} // namespace hpm
} // namespace nx
