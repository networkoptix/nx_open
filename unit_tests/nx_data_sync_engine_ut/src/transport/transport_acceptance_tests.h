#pragma once

#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/rest/http_rest_client.h>

#include "../cluster_test_fixture.h"

namespace nx::data_sync_engine::transport::test {

template<typename TransportTypeSet>
class TransportAcceptance:
    public ::testing::Test
{
    using Acceptor = typename TransportTypeSet::Acceptor;
    using CommandPipeline = typename TransportTypeSet::CommandPipeline;
    using Connector = typename TransportTypeSet::Connector;

public:
    ~TransportAcceptance()
    {
        for (auto& connector: m_connectors)
            connector->pleaseStopSync();
        m_connectors.clear();
    }

protected:
    virtual void SetUp() override
    {
        m_nodeCluster.addPeer();
    }

    void setMaximumActiveConnectionCountPerSystem(int /*value*/)
    {
        // TODO: #ak Currently, 1 is hardcoded in ConnectionManager.
        // Configure it here when hardcode is fixed.
    }

    void establishSameSystemConnectionsSimultaneosly(int count)
    {
        const auto systemId = QnUuid::createUuid().toSimpleByteArray().toStdString();
        auto url = m_nodeCluster.peer(0).syncronizationUrl();
        url.setPath(nx::network::http::rest::substituteParameters(
            url.path().toStdString(),
            {systemId}).c_str());

        const auto nodeId = QnUuid::createUuid().toSimpleByteArray().toStdString();

        for (int i = 0; i < count; ++i)
        {
            auto connector = std::make_unique<Connector>(
                ProtocolVersionRange::any,
                url,
                systemId,
                nodeId);
            m_connectors.push_back(std::move(connector));
            m_connectors.back()->connect(
                [this](auto&&... args) { processConnectResult(std::move(args)...); });
        }
    }

    void waitForEveryConnectToComplete()
    {
        const int count = (int) m_connectors.size();
        for (int i = 0; i < count; ++i)
        {
            auto connectResult = m_connectResults.pop();
            if (connectResult.connection)
                connectResult.connection->pleaseStopSync();
        }
    }

private:
    struct ConnectResult
    {
        ConnectResultDescriptor connectResultDescriptor;
        std::unique_ptr<AbstractCommandPipeline> connection;
    };

    data_sync_engine::test::ClusterTestFixture m_nodeCluster;
    std::vector<std::unique_ptr<AbstractCommandPipelineConnector>> m_connectors;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;

    void processConnectResult(
        ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<AbstractCommandPipeline> connection)
    {
        m_connectResults.push({connectResultDescriptor, std::move(connection)});
    }
};

TYPED_TEST_CASE_P(TransportAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(
    TransportAcceptance,
    transport_connection_is_removed_before_http_response_has_been_sent)
{
    setMaximumActiveConnectionCountPerSystem(1);
   
    constexpr int connectionToEstablishCount = 11;
    establishSameSystemConnectionsSimultaneosly(connectionToEstablishCount);

    waitForEveryConnectToComplete();
}

REGISTER_TYPED_TEST_CASE_P(TransportAcceptance,
    transport_connection_is_removed_before_http_response_has_been_sent
);

} // namespace nx::data_sync_engine::transport::test
