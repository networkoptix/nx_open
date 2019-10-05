#pragma once

#include <nx/clusterdb/map/embedded_database.h>

#include "mediator_endpoint.h"
#include "mediator_selector.h"

namespace nx::hpm {

namespace conf {

class Settings;
struct ListeningPeerDb;

}

/**
 * Associates peer domains (e.g. mediaserverid.systemid) with a mediator instance domain
 * discovering other mediator instances and synchronizing with their
 * ListeningPeerDbs.
 */
class ListeningPeerDb
{
public:
    ListeningPeerDb(const conf::Settings& settings);

    /**
     * Initializes the underlying database.
     */
    bool initialize();

    /**
     * stops the underlying database.
     */
    void stop();

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

private:
    void setThisMediatorEndpoint(const MediatorEndpoint& endpoint);

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
};

} // namespace nx::hpm
