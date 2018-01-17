#include "remote_relay_peer_pool.h"

#include <sstream>

#include <boost/algorithm/string.hpp>

#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {

namespace {

template<typename T>
bool getQueryResultValue(const cassandra::QueryResult& queryResult, const char* columnName, T* t)
{
    bool getValueResult = queryResult.get(columnName, t);
    NX_ASSERT(getValueResult);
    if (!getValueResult)
    {
        NX_LOG(lm("[RemoteRelayPeerPool] Get %1 from QueryResult failed").arg(columnName),
            cl_logDEBUG2);
        return false;
    }

    NX_ASSERT((bool)*t);
    if (!(bool)*t)
    {
        NX_LOG(lm("[RemoteRelayPeerPool] %1 is NULL").arg(columnName), cl_logDEBUG2);
        return false;
    }

    return true;
}

} // namespace


RemoteRelayPeerPool::RemoteRelayPeerPool(const conf::Settings& settings):
    m_settings(settings)
{
}

bool RemoteRelayPeerPool::connectToDb()
{
    m_cassConnection = 
        nx::cassandra::AsyncConnectionFactory::instance().create(
            m_settings.cassandraConnection().host);

    prepareDbStructure();

    if (!m_dbReady)
        m_cassConnection.reset();

    return m_dbReady;
}

void RemoteRelayPeerPool::prepareDbStructure()
{
    // TODO: Use DbStructureUpdater here (it will require some refactoring).
    NX_INFO(this, lm("Initiating connection to cassandra DB"));

    m_cassConnection->init()
        .then(
            [this](cf::future<CassError> result)
            {
                const auto resultCode = result.get();
                if (resultCode != CASS_OK)
                {
                    NX_INFO(this, lm("Error connecting to cassandra DB. "
                        "Connection initialization failed. %1").arg(resultCode));
                    return result;
                }

                return m_cassConnection->executeUpdate(
                    "CREATE KEYSPACE cdb WITH replication = { \
                        'class': 'SimpleStrategy', 'replication_factor': '2' };");
            })
        .then(
            [this](cf::future<CassError> result)
            {
                const auto resultCode = result.get();
                if (resultCode != CASS_OK)
                {
                    NX_INFO(this, lm("Error connecting to cassandra DB. "
                        "Error creating keyspace. %1").arg(resultCode));
                    return result;
                }

                return m_cassConnection->executeUpdate(
                    "CREATE TABLE cdb.relay_peers ( \
                        node_id             text, \
                        domain_suffix_1     text, \
                        domain_suffix_2     text, \
                        domain_suffix_3     text, \
                        domain_name_tail    text, \
                        PRIMARY KEY (domain_suffix_1, domain_suffix_2, domain_suffix_3, \
                            domain_name_tail) \
                    );");
            })
        .then(
            [this](cf::future<CassError> result)
            {
                const auto resultCode = result.get();
                if (resultCode == CASS_OK || resultCode == CASS_ERROR_SERVER_ALREADY_EXISTS)
                {
                    NX_INFO(this, "Connection to cassandra DB established. "
                        "DB structure prepared successfully");
                    m_dbReady = true;
                    return cf::unit();
                }

                NX_INFO(this, lm("Error connecting to cassandra DB. "
                    "Error creating tables. %1").arg(resultCode));
                return cf::unit();
            }).wait();
}

cf::future<std::string> RemoteRelayPeerPool::findRelayByDomain(const std::string& domainName) const
{
    if (!m_dbReady)
        return cf::make_ready_future(std::string());

    auto whereString = whereStringForFind(domainName);
    std::string queryString = "SELECT node_id FROM cdb.relay_peers " + whereString;
    NX_VERBOSE(this, lm("find relay: query string: %1").arg(queryString));

    return m_cassConnection->prepareQuery(queryString.c_str())
        .then(
            [this](cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto result = prepareFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_DEBUG(this,
                        lm("Error prepare select from cdb.relay_peers. %1").arg(result.first));
                    return cf::make_ready_future(
                        std::make_pair(result.first, cassandra::QueryResult()));
                }

                return m_cassConnection->executeSelect(std::move(result.second));
            })
        .then(
            [this, domainName](cf::future<std::pair<CassError, cassandra::QueryResult>> selectFuture)
            {
                auto result = selectFuture.get();
                if (result.first != CASS_OK)
                {
                    NX_DEBUG(this, lm("Error selecting from cdb.relay_peers. %1").arg(result.first));
                    return std::string();
                }

                boost::optional<std::string> relayHost;
                while (result.second.next())
                {
                    if (!getQueryResultValue(result.second, "node_id", &relayHost))
                        return std::string();

                    if (*relayHost == m_nodeId)
                    {
                        NX_VERBOSE(this, 
                            lm("find relay: selected host string is equal to this host string (%1)")
                                .arg(m_nodeId));
                        continue;
                    }

                    NX_VERBOSE(this, lm("Found relay host %1 for domain %2")
                        .arg(*relayHost)
                        .arg(domainName));

                    return *relayHost;
                }

                NX_VERBOSE(this, lm("Failed to find relay host for domain %2").arg(domainName));

                return std::string();
            });
}

cf::future<bool> RemoteRelayPeerPool::addPeer(const std::string& domainName)
{
    if (!m_dbReady)
        return cf::make_ready_future(false);

    NX_ASSERT(!m_nodeId.empty());
    if (m_nodeId.empty())
    {
        NX_ERROR(this, "Node id must be set before adding peers");
        return cf::make_ready_future(false);
    }

    return m_cassConnection->prepareQuery(
                    "INSERT INTO cdb.relay_peers ( \
                        node_id, \
                        domain_suffix_1, \
                        domain_suffix_2, \
                        domain_suffix_3, \
                        domain_name_tail ) VALUES(?, ?, ?, ?, ?);")
        .then(
            [this, domainName](
                cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto prepareResult = prepareFuture.get();
                if (prepareResult.first != CASS_OK)
                {
                    NX_DEBUG(this, lm("Error preparing insert into cdb.relay_peers. %1")
                        .arg(prepareResult.first));
                    return cf::make_ready_future(std::move((CassError)prepareResult.first));
                }

                if (!bindUpdateParameters(&prepareResult.second, domainName, UpdateType::add))
                {
                    NX_DEBUG(this, lm("Error binding parameters for insert into cdb.relay_peers"));
                    return cf::make_ready_future((CassError)CASS_ERROR_SERVER_INVALID_QUERY);
                }

                return m_cassConnection->executeUpdate(std::move(prepareResult.second));
            })
        .then(
            [this](cf::future<CassError> executeFuture)
            {
                const auto resultCode = executeFuture.get();
                if (resultCode != CASS_OK)
                {
                    NX_VERBOSE(this, 
                        lm("Error executing insert into cdb.relay_peers. %1").arg(resultCode));
                    return false;
                }

                NX_VERBOSE(this, "Execute insert into cdb.relay_peers succeded");
                return true;
            });
}

cf::future<bool> RemoteRelayPeerPool::removePeer(const std::string& domainName)
{
    if (!m_dbReady)
        return cf::make_ready_future(false);

    return m_cassConnection->prepareQuery(
                    "DELETE FROM cdb.relay_peers WHERE \
                        domain_suffix_1=? AND \
                        domain_suffix_2=? AND \
                        domain_suffix_3=? AND \
                        domain_name_tail = ?;")
        .then(
            [this, domainName](
                cf::future<std::pair<CassError, cassandra::Query>> prepareFuture)
            {
                auto prepareResult = prepareFuture.get();
                if (prepareResult.first != CASS_OK)
                {
                    NX_DEBUG(this, lm("Error preparing delete from cdb.relay_peers")
                        .arg(prepareResult.first));
                    return cf::make_ready_future(std::move((CassError)prepareResult.first));
                }

                if (!bindUpdateParameters(&prepareResult.second, domainName, UpdateType::remove))
                {
                    NX_DEBUG(this, "Error binding parameters for delete from cdb.relay_peers");
                    return cf::make_ready_future((CassError)CASS_ERROR_SERVER_INVALID_QUERY);
                }

                return m_cassConnection->executeUpdate(std::move(prepareResult.second));
            })
        .then(
            [this](cf::future<CassError> executeFuture)
            {
                const auto executeResult = executeFuture.get();
                if (executeFuture.get() != CASS_OK)
                {
                    NX_DEBUG(this, lm("Error executing delete from cdb.relay_peers. %1")
                        .arg(executeResult));
                    return false;
                }

                NX_VERBOSE(this, "Execute delete from cdb.relay_peers succeded");
                return true;
            });
}

void RemoteRelayPeerPool::setNodeId(const std::string& nodeId)
{
    m_nodeId = nodeId;
}

bool RemoteRelayPeerPool::bindUpdateParameters(
    cassandra::Query* query,
    const std::string& domainName,
    UpdateType updateType) const
{
    auto reversedDomainName = nx::utils::reverseWords(domainName, ".");
    std::vector<std::string> domainParts;
    boost::split(domainParts, reversedDomainName, boost::is_any_of("."));

    NX_ASSERT(!domainParts.empty());

    bool bindResult = true;

    if (updateType == UpdateType::add)
        bindResult &= query->bind("node_id", m_nodeId);

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
    ss << " WHERE ";

    auto addDomainSuffixParam =
        [&partsCount](std::stringstream* ss, const std::vector<std::string>& parts, int index,
            bool last = false)
        {
            if (partsCount > index)
                *ss << "domain_suffix_" << index + 1 << " = '" << parts[index] << "'";

            if (!last)
                *ss << " AND ";
        };

    addDomainSuffixParam(&ss, domainParts, 0, domainParts.size() <= 1);
    for (int i = 1; i < (int)std::min((size_t)3, domainParts.size()); ++i)
        addDomainSuffixParam(&ss, domainParts, i, i == (int)domainParts.size() - 1);

    if (domainParts.size() > 3)
    {
        ss << " domain_name_tail = '";
        for (int i = 3; i < partsCount; ++i)
        {
            ss << domainParts[i];
            if (i != partsCount - 1)
                ss << ".";
        }
        ss << "'";
    }

    ss << ";";
    return ss.str();
}

cassandra::AbstractAsyncConnection* RemoteRelayPeerPool::getConnection()
{
    return m_cassConnection.get();
}

RemoteRelayPeerPool::~RemoteRelayPeerPool()
{
    if (m_dbReady)
        m_cassConnection->wait();
}

//-------------------------------------------------------------------------------------------------

static RemoteRelayPeerPoolFactory::FactoryFunc remoteRelayPeerPoolFactoryFunc;

std::unique_ptr<model::AbstractRemoteRelayPeerPool> RemoteRelayPeerPoolFactory::create(
    const conf::Settings& settings)
{
    if (remoteRelayPeerPoolFactoryFunc)
        return remoteRelayPeerPoolFactoryFunc(settings);

    return std::make_unique<model::RemoteRelayPeerPool>(settings);
}

void RemoteRelayPeerPoolFactory::setFactoryFunc(FactoryFunc func)
{
    remoteRelayPeerPoolFactoryFunc.swap(func);
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
