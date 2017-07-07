#include "remote_relay_peer_pool.h"
#include <nx/utils/log/log.h>
#include <nx/casssandra/async_cassandra_connection.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

RemoteRelayPeerPool::RemoteRelayPeerPool():
    m_cassConnection(new nx::cassandra::AsyncConnection("127.0.0.1"))
{
    prepareDbStructure();
}

void RemoteRelayPeerPool::prepareDbStructure()
{
    m_cassConnection->init()
        .then(
            [this](cf::future<CassError> result)
            {
                if (result.get() != CASS_OK)
                    return result;

                return m_cassConnection->executeUpdate(
                    "CREATE KEYSPACE cdb WITH replication = { \
                        'class': 'SimpleStrategy', 'replication_factor': '2' };");
            })
        .then(
            [this](cf::future<CassError> result)
            {
                if (result.get() != CASS_OK)
                    return result;

                return m_cassConnection->executeUpdate(
                    "CREATE TABLE cdb.relay_servers ( \
                        relay_id            text, \
                        domain_suffix_1     text, \
                        domain_suffix_2     text, \
                        domain_suffix_3     text, \
                        domain_name_tail    text, \
                        PRIMARY KEY (relay_id, domain_suffix_1, domain_suffix_2, domain_suffix_3) \
                    );");
            })
        .then(
            [this](cf::future<CassError> result)
            {
                if (result.get() == CASS_OK || result.get() == CASS_ERROR_SERVER_ALREADY_EXISTS)
                {
                    NX_VERBOSE(this, "Prepare db successful");
                    m_dbReady = true;
                }

                NX_VERBOSE(this, "Prepare db failed");
                return cf::unit();
            }).wait();
}

cf::future<std::string> RemoteRelayPeerPool::findRelayByDomain(
        const std::string& /*domainName*/) const
{
    if (!m_dbReady)
        return cf::make_ready_future(std::string());

    struct Context
    {
        std::string localAddress;
    };

    auto sharedContext = std::make_shared<Context>();

    return m_cassConnection->executeSelect("SELECT listen_address from system.local;")
        .then(
            [this, sharedContext](
                cf::future<std::pair<CassError, cassandra::QueryResult>> resultFuture)
            {
                auto result = resultFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Select local address failed");
                    return cf::make_ready_future(std::make_pair(result.first, cassandra::Query()));
                }

                if (!result.second.next())
                {
                    NX_VERBOSE(this, "Select local address failed. Empty cursor.");
                    return cf::make_ready_future(
                        std::make_pair(CASS_ERROR_SERVER_INVALID_QUERY, cassandra::Query()));
                }

                auto getValueResult = result.second.get(0, &sharedContext->localAddress);
                if (!getValueResult)
                {
                    NX_VERBOSE(this, "Get local address from QueryResult failed");
                    return cf::make_ready_future(
                        std::make_pair(CASS_ERROR_SERVER_INVALID_QUERY, cassandra::Query()));
                }

                return m_cassConnection->prepareQuery(
                    "SELECT relay_id, domain_suffix_1, domain_suffix_2, \
                            domain_suffix_3, domain_name_tail \
                     FROM cdb.relay_servers \
                     WHERE relay_id != ?;");
            })
        .then(
            [this, sharedContext](cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto result = prepareFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Prepare select from cdb.relay_servers failed");
                    return cf::make_ready_future(
                        std::make_pair(result.first, cassandra::QueryResult()));
                }

                result.second.bind("relay_id", sharedContext->localAddress);
                return m_cassConnection->executeSelect(std::move(result.second));
            })
        .then(
            [this](cf::future<std::pair<CassError, cassandra::QueryResult>> selectFuture)
            {
                auto result = selectFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Select from cdb.relay_servers failed");
                    return std::string();
                }

                if (!result.second.next())
                {
                    NX_VERBOSE(this, "Select from cdb.relay_servers failed. Empty cursor.");
                    return std::string();
                }

                std::string relayId;
                bool getValueResult = result.second.get(std::string("relay_id"), &relayId);
                if (!getValueResult)
                {
                    NX_VERBOSE(this, "Get relay_id from QueryResult failed");
                    return std::string();
                }

                return relayId;
            });
}

bool RemoteRelayPeerPool::addPeer(const std::string& /*domainName*/, const std::string& /*peerName*/)
{
//    if (!m_dbReady)
        return false;
}


} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
