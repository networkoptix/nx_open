#pragma once

#include <nx/clusterdb/map/embedded_database.h>

#include "settings.h"

namespace nx::hpm {

/**
 * Represents the endpoint that a mediator instance listens on,
 * including http(or https) and stun udp ports.
 */
struct MediatorEndpoint
{
    std::string domainName; //< Ip address without port or domain name to be resolved by dns
    int httpPort;
    int httpsPort;
    int stunUdpPort;

    bool operator ==(const MediatorEndpoint& other) const;
};

//-------------------------------------------------------------------------------------------------
// RemoteMediatorPeerPool

/**
 * Associates peer domains with a mediator instance domain
 * discovering other mediator instances and synchronizing with their
 * RemoteMediatorPeerPools.
 */
class RemoteMediatorPeerPool
{
public:
    RemoteMediatorPeerPool(const conf::ClusterDbMap& settings);

    /**
     * Initializes the underlying database.
     */
    bool initialize();

    /**
     * Sets the domain name that all added peers will be associated with.
     * NOTE: MUST be called before addPeer or startDiscovery can be called.
     */
    void setEndpoint(const MediatorEndpoint& endpoint);

    /**
     * Adds or updates the domain name of a peer with the public url of the mediator instance given
     * in the constructor to the peer pool.
     * NOTE: setEndpoint MUST also be called before addPeer can be called.
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
     * their RemoteMediatorPeerPool entries.
     * NOTE: setEndpoint MUST be called before startDiscovery will work.
     *
     * @return true if discovery service started successfully, false otherwise
     */
    void startDiscovery(
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * Get this mediator instance's endpoint.
     */
    const MediatorEndpoint& thisEndpoint() const;

private:
    const conf::ClusterDbMap& m_settings;
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlExecutor;
    std::unique_ptr<nx::clusterdb::map::EmbeddedDatabase> m_map;

    MediatorEndpoint m_mediatorEndpoint;
    std::string m_mediatorEndpointString;
    nx::utils::Url m_syncEngineUrl;
};

} // namespace nx::hpm
