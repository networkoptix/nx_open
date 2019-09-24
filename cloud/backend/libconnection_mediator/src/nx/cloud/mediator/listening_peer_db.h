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
 * Associates peers (e.g. mediaserverid.systemid) with this mediator instance
 * and discovers other mediators and synchronizing with their ListeningPeerDbs.
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
	 * @param peerId should be of the form serverId.systemId
     */
    void addPeer(
        const std::string& peerId,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Removes the peer from the peer pool.
	 * @param peerId should be of the form serverId.systemId
     */
    void removePeer(
        const std::string& peerId,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Get domain name of the mediator that the given peerId is registered with.
     * If no domain name is found, a MediatorEndpoint with empty values is provided.
	 * @param peerId should be of the form serverId.systemId
     */
    void findMediatorByPeerDomain(
        const std::string& peerId,
        nx::utils::MoveOnlyFunc<void(MediatorEndpoint)> handler);

    /**
     * Adds or updates the connectionSpeed for the given peerId.
     * handler receives true if successful, false otherwise.
	 * @param peerId should be of the form serverId.systemId
     */
    void addUplinkSpeed(
        const std::string& peerId,
        const nx::hpm::api::ConnectionSpeed& uplinkSpeed,
        nx::utils::MoveOnlyFunc<void(bool)> handler);

    /**
     * Get the status of the listening peer specified by peerId ([serverId.]systemId)
     * Optionally, if only systemId is given, the map will contain the ListeningPeerStatus for all
     * peers with that systemId. if serverId is given and exists, the map returned will contain
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
	struct RemovePeerContext
	{
		std::vector<std::string> keys;
		nx::utils::MoveOnlyFunc<void(bool)> handler;
		std::atomic_int keysRemoved = 0;
		std::atomic_bool ok = true;
	};

	/**
	 * Constructs a unique key of the form:
	 * "systemId.serverId#<mediatorEndpointString>#mediatorEndpoint", where <mediatorEndpointString>
	 * is a string with this mediators domain name and stun, http, and https ports.
	 * NOTE: <mediatorEndpointString> is included to ensure uniqueness. Peers can connect to
	 * multiple mediators, meaning that this key could appear multiple times in the database as
	 * the mediators synchronize with eachother.
	 * @param peerId should be of the form serverId.systemId
	 */
    std::string mediatorEndpointKey(const std::string& peerId) const;

	/**
	 * Constructs a unique key of the form: "systemId.serverId#uplinkSpeed"
	 * @param peerId should be of the form serverId.systemId
	 */
	std::string uplinkSpeedKey(const std::string& peerId) const;

	/**
	 * Builds this mediators infojson consisting of its tcp and upd urls that get advertised to
	 * the DiscoveryService. the DiscoveryService uses them to provide cloud_modules.xml to peers
	 * that are trying to find a mediator to connect to.
	 */
    std::string buildInfoJson(const MediatorEndpoint& endpoint) const;

	void removePeer(std::shared_ptr<RemovePeerContext> context);

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
