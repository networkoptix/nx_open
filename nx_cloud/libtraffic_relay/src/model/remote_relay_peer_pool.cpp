#include <sstream>
#include <boost/algorithm/string.hpp>
#include "remote_relay_peer_pool.h"
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/casssandra/async_cassandra_connection.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {


RemoteRelayPeerPool::RemoteRelayPeerPool(const char* cassandraHost):
    m_cassConnection(new nx::cassandra::AsyncConnection(cassandraHost))
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
                    "CREATE TABLE cdb.relay_peers ( \
                        relay_id            uuid, \
                        domain_suffix_1     text, \
                        domain_suffix_2     text, \
                        domain_suffix_3     text, \
                        domain_name_tail    text, \
                        PRIMARY KEY (relay_id, domain_suffix_1, domain_suffix_2, domain_suffix_3, \
                            domain_name_tail) \
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

cf::future<int> RemoteRelayPeerPool::getLocalHostId() const
{
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_hostId.empty())
            return cf::make_ready_future((int)CASS_OK);
    }

    return m_cassConnection->prepareQuery("SELECT host_id from system.local;")
        .then(
            [this](cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto prepareResult = prepareFuture.get();
                if (prepareResult.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Prepare select local host id failed");
                    return cf::make_ready_future(
                        std::make_pair(prepareResult.first, cassandra::QueryResult()));
                }

                return m_cassConnection->executeSelect(std::move(prepareResult.second));
            })
        .then(
            [this](
                cf::future<std::pair<CassError, cassandra::QueryResult>> selectFuture)
            {
                auto result = selectFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Select local host id failed");
                    return (int)result.first;
                }

                if (!result.second.next())
                {
                    NX_VERBOSE(this, "Select local host id failed. Empty cursor.");
                    return (int)CASS_ERROR_SERVER_INVALID_QUERY;
                }

                boost::optional<cassandra::Uuid> localHostId;
                auto getValueResult = result.second.get(0, &localHostId);
                if (!getValueResult)
                {
                    NX_VERBOSE(this, "Get local host id from QueryResult failed");
                    return (int)CASS_ERROR_SERVER_INVALID_QUERY;
                }

                if (!(bool)localHostId)
                {
                    NX_VERBOSE(this, "Local host id is NULL");
                    return (int)CASS_ERROR_SERVER_INVALID_QUERY;
                }

                {
                    QnMutexLocker lock(&m_mutex);
                    m_hostId = localHostId->uuidString;
                }

                return (int)CASS_OK;
            });
}

cf::future<std::string> RemoteRelayPeerPool::findRelayByDomain(
        const std::string& domainName) const
{
    if (!m_dbReady)
        return cf::make_ready_future(std::string());

    return getLocalHostId()
        .then(
            [this, domainName](cf::future<int> getLocalHostIdFuture)
            {
                auto errorCode = (CassError)getLocalHostIdFuture.get();
                if (errorCode != CASS_OK)
                    return cf::make_ready_future(std::make_pair(errorCode, cassandra::Query()));

                auto whereString = whereStringForFind(domainName);

                return m_cassConnection->prepareQuery(
                    ("SELECT relay_id \
                     FROM cdb.relay_peers " + whereString).c_str());
            })
        .then(
            [this](cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto result = prepareFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Prepare select from cdb.relay_peers failed");
                    return cf::make_ready_future(
                        std::make_pair(result.first, cassandra::QueryResult()));
                }

                cassandra::Uuid localHostId;
                {
                    QnMutexLocker lock(&m_mutex);
                    localHostId.uuidString = m_hostId;
                }

                if (!result.second.bind("relay_id", localHostId))
                {
                    NX_VERBOSE(this, lm("Bind local host id %1 to relay_id failed")
                        .arg(localHostId.uuidString));
                    return cf::make_ready_future(
                        std::make_pair(CASS_ERROR_SERVER_INVALID_QUERY, cassandra::QueryResult()));
                }

                return m_cassConnection->executeSelect(std::move(result.second));
            })
        .then(
            [this](cf::future<std::pair<CassError, cassandra::QueryResult>> selectFuture)
            {
                auto result = selectFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Select from cdb.relay_peers failed");
                    return std::string();
                }

                if (!result.second.next())
                {
                    NX_VERBOSE(this, "Select from cdb.relay_peers failed. Empty cursor.");
                    return std::string();
                }

                boost::optional<std::string> relayId;
                bool getValueResult = result.second.get(std::string("relay_id"), &relayId);
                if (!getValueResult)
                {
                    NX_VERBOSE(this, "Get relay_id from QueryResult failed");
                    return std::string();
                }

                NX_ASSERT((bool) relayId);
                if (!(bool)relayId)
                {
                    NX_VERBOSE(this, "relay_id is NULL");
                    return std::string();
                }

                return *relayId;
            });
}

cf::future<bool> RemoteRelayPeerPool::addPeer(const std::string& domainName)
{
    if (!m_dbReady)
        return cf::make_ready_future(false);

    return getLocalHostId()
        .then(
            [this](cf::future<int> getLocalHostIdFuture)
            {
                auto errorCode = (CassError)getLocalHostIdFuture.get();
                if (errorCode != CASS_OK)
                    return cf::make_ready_future(std::make_pair(errorCode, cassandra::Query()));

                return m_cassConnection->prepareQuery(
                    "INSERT INTO cdb.relay_peers ( \
                        relay_id, \
                        domain_suffix_1, \
                        domain_suffix_2, \
                        domain_suffix_3, \
                        domain_name_tail ) VALUES(?, ?, ?, ?, ?);");
            })
        .then(
            [this, domainName](
                cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto prepareResult = prepareFuture.get();
                if (prepareResult.first != CASS_OK)
                {
                    NX_VERBOSE(this, "Prepare insert into cdb.relay_peers failed");
                    return cf::make_ready_future(std::move((CassError)prepareResult.first));
                }

                if (!bindInsertParameters(&prepareResult.second, domainName))
                {
                    NX_VERBOSE(this, "Bind parameters for insert into cdb.relay_peers failed");
                    return cf::make_ready_future((CassError)CASS_ERROR_SERVER_INVALID_QUERY);
                }

                return m_cassConnection->executeUpdate(std::move(prepareResult.second));
            })
        .then(
            [this](cf::future<CassError> executeFuture)
            {
                if (executeFuture.get() != CASS_OK)
                {
                    NX_VERBOSE(this, "Execute insert into cdb.relay_peers failed");
                    return false;
                }

                NX_VERBOSE(this, "Execute insert into cdb.relay_peers succeded");
                return true;
            });
}

bool RemoteRelayPeerPool::bindInsertParameters(
    cassandra::Query* query,
    const std::string& domainName) const
{
    auto reversedDomainName = nx::utils::reverseWords(domainName, ".");
    std::vector<std::string> domainParts;
    boost::split(domainParts, reversedDomainName, boost::is_any_of("."));

    NX_ASSERT(!domainParts.empty());

    bool bindResult = true;
    cassandra::Uuid localHostId;
    {
        QnMutexLocker lock(&m_mutex);
        localHostId.uuidString = m_hostId;
    }

    bindResult &= query->bind("relay_id", localHostId);
    bindResult &= query->bind("domain_suffix_1", domainParts[0]);
    bindResult &= query->bind("domain_suffix_2", domainParts.size() > 1 ? domainParts[1] : "");
    bindResult &= query->bind("domain_suffix_3", domainParts.size() > 2 ? domainParts[2] : "");

    if (domainParts.size() > 3)
    {
        std::stringstream ss;
        for (int i = 3; i < (int)domainParts.size(); ++i)
        {
            ss << domainParts[i];
            if (i != (int)domainParts.size() - 1)
                ss << ".";
        }
        bindResult &= query->bind("domain_name_tail", ss.str());
    }
    else
    {
        bindResult &= query->bind("domain_name_tail", std::string(""));
    }

    return bindResult;
}


std::string RemoteRelayPeerPool::whereStringForFind(const std::string& domainName) const
{
    auto reversedDomainName = nx::utils::reverseWords(domainName, ".");

    std::vector<std::string> domainParts;
    boost::split(domainParts, reversedDomainName, boost::is_any_of("."));
    int partsCount = (int)domainParts.size();

    std::stringstream ss;
    ss << " WHERE relay_id != ? ";

    auto addDomainSuffixParam =
        [&partsCount](std::stringstream* ss, const std::vector<std::string>& parts,
            int index, bool first = false)
        {
            if (first)
                *ss << " AND ";

            if (partsCount > index)
                *ss << "domain_suffix_" << index + 1 << " = '" << parts[index] << "'";
        };

    addDomainSuffixParam(&ss, domainParts, 0, true);
    for (int i = 1; i < 3; ++i)
        addDomainSuffixParam(&ss, domainParts, i);

    if (domainParts.size() > 3)
    {
        ss << " AND domain_name_tail = ";
        for (int i = 3; i < partsCount; ++i)
        {
            ss << domainParts[i];
            if (i != partsCount - 1)
                ss << ".";
        }
    }

    ss << ";";
    return ss.str();
}

cassandra::AsyncConnection* RemoteRelayPeerPool::getConnection()
{
    return m_cassConnection.get();
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
