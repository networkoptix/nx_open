#include "cluster_test_fixture.h"

#include <future>

#include <nx/network/url/url_builder.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>

#include <nx/clusterdb/engine/http/http_paths.h>

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

nx::utils::Url Peer::baseApiUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_process.moduleInstance()->httpEndpoints().front());
}

nx::utils::Url Peer::syncronizationUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_process.moduleInstance()->httpEndpoints().front())
        .setPath(kBaseSynchronizationPath);
}

void Peer::connectTo(const Peer& other)
{
    const auto url = network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(other.process().moduleInstance()->httpEndpoints().front());

    m_process.moduleInstance()->connectToNode(
        /*systemId*/ "review_and_replace_this_value",
        url);
}

Customer Peer::addRandomData()
{
    Customer customer;
    customer.id = QnUuid::createUuid().toSimpleByteArray().toStdString();
    customer.fullName = nx::utils::generateRandomName(7).toStdString();
    customer.address = nx::utils::generateRandomName(7).toStdString();

    std::promise<ResultCode> done;
    process().moduleInstance()->customerManager().saveCustomer(
        customer,
        [&done](ResultCode resultCode) { done.set_value(resultCode); });
    const auto resultCode = done.get_future().get();
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

void Peer::setOutgoingCommandFilter(const OutgoingCommandFilterConfiguration& filter)
{
    process().moduleInstance()->syncronizationEngine().setOutgoingCommandFilter(filter);
}

//-------------------------------------------------------------------------------------------------

ClusterTestFixture::ClusterTestFixture():
    base_type("nx_data_sync_engine_ut", "")
{
}

void ClusterTestFixture::addPeer()
{
    const auto dbFileArg = lm("--db/name=%1/db_%2.sqlite")
        .args(testDataDir(), ++m_peerCounter).toStdString();

    auto peer = std::make_unique<Peer>();
    peer->process().addArg(dbFileArg.c_str());

    ASSERT_TRUE(peer->process().startAndWaitUntilStarted());
    m_peers.push_back(std::move(peer));
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
