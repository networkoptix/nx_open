#include "cluster_test_fixture.h"

#include <future>

#include <nx/network/url/url_builder.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>

namespace nx::clusterdb::engine::test {

Peer::Peer():
    m_nodeId(QnUuid::createUuid().toSimpleString().toStdString())
{
    m_process.addArg("-p2pDb/nodeId", m_nodeId.c_str());
}

nx::utils::test::ModuleLauncher<CustomerDbNode>& Peer::process()
{
    return m_process;
}

const nx::utils::test::ModuleLauncher<CustomerDbNode>& Peer::process() const
{
    return m_process;
}

std::string Peer::nodeId() const
{
    return m_nodeId;
}

nx::utils::Url Peer::baseApiUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_process.moduleInstance()->httpEndpoints().front());
}

void Peer::connectTo(const Peer& other)
{
    const auto url = network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(other.process().moduleInstance()->httpEndpoints().front());

    m_process.moduleInstance()->connectToNode(url);
}

bool Peer::isConnectedTo(const Peer& other) const
{
    const auto connections =
        process().moduleInstance()->synchronizationEngine().connectionManager().getConnections();
    const auto& otherEndpoints = other.process().moduleInstance()->httpEndpoints();
    for (const auto& connection: connections)
    {
        auto endpointsEqual =
            [&connection](const auto& endpoint)
            {
                return connection.peerEndpoint.toStdString() == endpoint.toStdString();
            };

        if (std::any_of(otherEndpoints.begin(), otherEndpoints.end(), endpointsEqual))
            return true;
    }

    return false;
}

void Peer::disconnectFrom(const Peer& other)
{
    const auto url = network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(other.process().moduleInstance()->httpEndpoints().front());

    m_process.moduleInstance()->disconnectFromNode(url);
}

Customer Peer::addRandomData()
{
    Customer customer;
    customer.id = QnUuid::createUuid().toSimpleByteArray().toStdString();
    customer.fullName = nx::utils::generateRandomName(7).toStdString();
    customer.address = nx::utils::generateRandomName(7).toStdString();

    const auto resultCode =
        process().moduleInstance()->customerManager().saveCustomer(customer);
    if (resultCode != ResultCode::ok)
        throw std::runtime_error(toString(resultCode));

    return customer;
}

bool Peer::hasData(const std::vector<Customer>& expected)
{
    std::promise<std::tuple<ResultCode, Customers>> done;
    process().moduleInstance()->customerManager().getCustomers(
        [&done](ResultCode resultCode, Customers customers)
        {
            done.set_value(std::make_tuple(resultCode, std::move(customers)));
        });
    auto [resultCode, customers] = done.get_future().get();
    if (resultCode != ResultCode::ok)
        return false;

    for (const auto& customer: expected)
    {
        if (!std::any_of(customers.begin(), customers.end(),
                [customer](const auto& value) { return value == customer; }))
        {
            return false;
        }
    }

    return true;
}

Customer Peer::modifyRandomly(const Customer& data)
{
    Customer modified = data;
    modified.fullName = nx::utils::generateRandomName(7).toStdString();
    modified.address = nx::utils::generateRandomName(7).toStdString();

    const auto resultCode =
        process().moduleInstance()->customerManager().saveCustomer(modified);
    if (resultCode != ResultCode::ok)
        throw std::runtime_error(toString(resultCode));

    return modified;
}

void Peer::setOutgoingCommandFilter(const OutgoingCommandFilterConfiguration& filter)
{
    process().moduleInstance()->synchronizationEngine().setOutgoingCommandFilter(filter);
}

//-------------------------------------------------------------------------------------------------

ClusterTestFixture::ClusterTestFixture():
    base_type("nx_data_sync_engine_ut", ""),
    m_clusterId("test_cluster_id")
{
}

std::string ClusterTestFixture::clusterId() const
{
    return m_clusterId;
}

Peer& ClusterTestFixture::addPeer(bool startAndWaitUntilStarted)
{
    const auto dbFileArg = lm("--db/name=%1/db_%2.sqlite")
        .args(testDataDir(), ++m_peerCounter).toStdString();

    auto peer = std::make_unique<Peer>();
    peer->process().addArg(dbFileArg.c_str());
    peer->process().addArg("-p2pDb/clusterId", m_clusterId.c_str());

    if (startAndWaitUntilStarted)
    {
        [this, &peer]() { ASSERT_TRUE(peer->process().startAndWaitUntilStarted()); }();
    }

    m_peers.push_back(std::move(peer));
    return *m_peers.back();
}

Peer& ClusterTestFixture::peer(int index)
{
    return *m_peers[index];
}

const Peer& ClusterTestFixture::peer(int index) const
{
    return *m_peers[index];
}

int ClusterTestFixture::peerCount() const
{
    return static_cast<int>(m_peers.size());
}

bool ClusterTestFixture::allPeersAreSynchronized() const
{
    std::vector<int> ids(m_peers.size(), 0);
    int lastId = 0;
    for (auto& id: ids)
        id = lastId++;

    return peersAreSynchronized(ids);
}

bool ClusterTestFixture::peersAreSynchronized(std::vector<int> ids) const
{
    std::optional<Customers> firstPeerCustomers;
    for (const auto& peerIndex: ids)
    {
        const auto& peer = m_peers[peerIndex];

        std::promise<std::tuple<ResultCode, Customers>> done;
        peer->process().moduleInstance()->customerManager().getCustomers(
            [&done](ResultCode resultCode, Customers customers)
            {
                done.set_value(std::make_tuple(resultCode, std::move(customers)));
            });
        auto result = done.get_future().get();
        if (std::get<0>(result) != ResultCode::ok)
            return false;
        const auto& nodeCustomers = std::get<1>(result);

        if (!firstPeerCustomers)
            firstPeerCustomers = std::move(nodeCustomers);
        else if (!(*firstPeerCustomers == nodeCustomers))
            return false;
    }

    return true;
}

} // namespace nx::clusterdb::engine::test
