#pragma once

#include <nx/clusterdb/map/embedded_database.h>

#include "settings.h"

namespace nx::hpm {

/**
 * Represents the endpoint that a mediator instance listens on,
 * including http(or https) and stun udp ports. A given port is set to -1 if unused.
 */
struct MediatorEndpoint
{
    static constexpr int kPortUnused = -1;

    std::string domainName; //< Ip address without port, or domain name to be resolved by dns
    int httpPort = kPortUnused;
    int httpsPort = kPortUnused;
    int stunUdpPort = kPortUnused;

    bool operator ==(const MediatorEndpoint& other) const;
};

//-------------------------------------------------------------------------------------------------
// ListeningPeerDb

/**
 * Associates peer domains (e.g. mediaserverid.systemid) with a mediator instance domain
 * discovering other mediator instances and synchronizing with their
 * ListeningPeerDbs.
 */
class ListeningPeerDb
{
public:
    ListeningPeerDb(const conf::ListeningPeerDb& settings);

    /**
     * Initializes the underlying database.
     */
    bool initialize();

    /**
     * Sets the domain name that all added peers will be associated with.
     * NOTE: MUST be called before addPeer or startDiscovery can be called.
     */
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

    /*
     * Starts discovery of other mediator instances and synchronizes
     * their ListeningPeerDb entries.
     * NOTE: setThisMediatorEndpoint MUST be called before startDiscovery will work.
     *
     * @return true if discovery service started successfully, false otherwise
     */
    void startDiscovery(
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * Get the node id of the underlying synchronizationEngine or an empty string if initialize()
     * failed.
     */
    std::string nodeId() const;

private:
    const conf::ListeningPeerDb& m_settings;
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlExecutor;
    std::unique_ptr<nx::clusterdb::map::EmbeddedDatabase> m_map;

    MediatorEndpoint m_mediatorEndpoint;
    std::string m_mediatorEndpointString;
    nx::utils::Url m_syncEngineUrl;
};

} // namespace nx::hpm
