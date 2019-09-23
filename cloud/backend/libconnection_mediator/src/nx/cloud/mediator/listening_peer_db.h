#pragma once

#include <nx/clusterdb/map/embedded_database.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/cloud/mediator/api/connection_speed.h>
#include <nx/utils/subscription.h>

#include "mediator_endpoint.h"
#include "mediator_selector.h"

namespace nx::hpm {

namespace conf {

class Settings;
struct ListeningPeerDb;

} // namespace conf

struct ListeningPeerStatus
{
    std::string systemId;
    std::string serverId;

    // The list of Mediators that a listeningPeer is connected to.
    std::vector<MediatorEndpoint> connectedEndpoints;
    // The listeningPeer's uplink speed.
    nx::hpm::api::ConnectionSpeed uplinkSpeed;
};

/**
 * Associates peer domains (e.g. mediaserverid.systemid) with a mediator instance domain
 * discovering other mediator instances and synchronizing with their ListeningPeerDbs.
 */
class ListeningPeerDb
{
public:
    ListeningPeerDb(const conf::Settings& settings);
    ~ListeningPeerDb();

    /**
     * Initializes the underlying database.
     * NOTE: Any async call made before initialize() returns successfully will be done in the same
     * thread as the calling function.
     */
    bool initialize();

    /**
     * Stops the underlying database. All further async calls will return immediately in the same
     * thread as the calling function until initialize is called again.
     */
    void stop();

    void setThisMediatorEndpoint(const MediatorEndpoint& endpoint);

    /**
     * Get this mediator instance's endpoint.
     */
    const MediatorEndpoint& thisMediatorEndpoint() const;

    /**
     * Adds or updates the domain name of a peer with the public url of the mediator instance given
     * in the constructor to the peer pool.
     * NOTE: setThisMediatorEndpoint MUST also be called before addPeer can be called.
     */
    void addPeer(
        const std::string& peerDomainName,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Removes the peer domain from the peer pool.
     */
    void removePeer(
        const std::string& peerDomainName,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Get domain name of the mediator that the given peerDomainName is registered with.
     * If no domain name is found, a MediatorEndpoint with empty values is provided.
     */
    void findMediatorByPeerDomain(
        const std::string& peerDomainName,
        nx::utils::MoveOnlyFunc<void(MediatorEndpoint)> handler);

    /**
     * Adds or updates the connectionSpeed for the given peerId (serverId[.systemId]).
     * handler receives true if successful, false otherwise.
     */
    void addUplinkSpeed(
        const std::string& peerId,
        const nx::hpm::api::ConnectionSpeed& uplinkSpeed,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Get the status of the listening peer specified by peerId ([serverId.]systemId])
     * Optionally, if only systemId is given, the map will contain the ListeningPeerStatus for all
     * peers with that systemId. if serverId is given and present, the map returned will contain
     * one entry.
     * @param peerId either serverId.systemId or systemId.
     * NOTE: the peerId key for each entry in the map is lower case.
     * NOTE: the peerId key is always serverId.systemId, needed to uniquely identify each peer.
     */
    std::map<std::string, ListeningPeerStatus> getListeningPeerStatus(
        const std::string& peerId) const;

    /**
     * Starts discovery of other mediator instances and synchronizes
     * their ListeningPeerDb entries.
     *
     * @return true if discovery service started successfully, false otherwise
     */
    void startDiscovery(
        const MediatorEndpoint& endpoint,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * Get the node id of the underlying synchronizationEngine or an empty string if initialize()
     * failed.
     */
    std::string nodeId() const;

    void subscribeToUplinkSpeedUpdated(
        nx::utils::MoveOnlyFunc<void(nx::hpm::api::PeerConnectionSpeed)> handler,
		nx::utils::SubscriptionId* const outId);
    void unsubscribeFromUplinkSpeedUpdated(nx::utils::SubscriptionId id);

private:

    std::string toInternalStorageFormat(const std::string& peerId) const;

    std::string buildInfoJson(const MediatorEndpoint& endpoint) const;

private:
    const conf::ListeningPeerDb& m_settings;
    std::unique_ptr<AbstractMediatorSelector> m_mediatorSelector;
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlExecutor;
    std::unique_ptr<nx::clusterdb::map::EmbeddedDatabase> m_map;

    MediatorEndpoint m_mediatorEndpoint;
    std::string m_mediatorEndpointString;
    nx::utils::Url m_syncEngineUrl;

    nx::utils::Subscription<nx::hpm::api::PeerConnectionSpeed> m_uplinkSpeedUpdated;
    std::atomic_bool m_stopped = true;
};

} // namespace nx::hpm
