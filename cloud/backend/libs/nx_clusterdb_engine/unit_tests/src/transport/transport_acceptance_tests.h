#pragma once

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/rest/http_rest_client.h>

#include "../cluster_test_fixture.h"

namespace nx::clusterdb::engine::transport::test {

template<typename TypeSet>
class CommandPipelineAcceptance:
    public ::testing::Test
{
    using Acceptor = typename TypeSet::Acceptor;
    using CommandPipeline = typename TypeSet::CommandPipeline;
    using Connector = typename TypeSet::Connector;

public:
    CommandPipelineAcceptance()
    {
        m_systemId = QnUuid::createUuid().toSimpleByteArray().toStdString();
        m_nodeId = QnUuid::createUuid().toSimpleByteArray().toStdString();
    }

    ~CommandPipelineAcceptance()
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
        for (int i = 0; i < count; ++i)
            whenConnect();
    }

    void whenConnect()
    {
        auto connector = std::make_unique<Connector>(
            ProtocolVersionRange::any,
            getUrlForSystem(m_systemId),
            m_systemId,
            m_nodeId);
        m_connectors.push_back(std::move(connector));
        m_connectors.back()->connect(
            [this](auto&&... args) { processConnectResult(std::move(args)...); });
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

    void thenConnectSucceeded()
    {
        thenConnectCompleted();

        ASSERT_NE(nullptr, m_prevConnectResult->connection);
        ASSERT_TRUE(m_prevConnectResult->result.ok());
    }

    void thenConnectCompleted()
    {
        m_prevConnectResult = m_connectResults.pop();
    }

    void andConnectionIsKnownOnNode()
    {
        ASSERT_TRUE(m_nodeCluster.peer(0).process().moduleInstance()->syncronizationEngine()
            .connectionManager().isSystemConnected(m_systemId));
    }

private:
    struct ConnectResult
    {
        ConnectResultDescriptor result;
        std::unique_ptr<AbstractCommandPipeline> connection;

        ConnectResult(ConnectResult&&) = default;
        ConnectResult& operator=(ConnectResult&&) = default;

        ~ConnectResult()
        {
            if (connection)
                connection->pleaseStopSync();
        }
    };

    clusterdb::engine::test::ClusterTestFixture m_nodeCluster;
    std::vector<std::unique_ptr<AbstractCommandPipelineConnector>> m_connectors;
    nx::utils::SyncQueue<ConnectResult> m_connectResults;
    std::optional<ConnectResult> m_prevConnectResult;
    std::string m_systemId;
    std::string m_nodeId;

    nx::utils::Url getUrlForSystem(const std::string& systemId)
    {
        auto url = m_nodeCluster.peer(0).syncronizationUrl();
        url.setPath(nx::network::http::rest::substituteParameters(
            url.path().toStdString(),
            {systemId}).c_str());
        return url;
    }

    void processConnectResult(
        ConnectResultDescriptor connectResultDescriptor,
        std::unique_ptr<AbstractCommandPipeline> connection)
    {
        m_connectResults.push({connectResultDescriptor, std::move(connection)});
    }
};

TYPED_TEST_CASE_P(CommandPipelineAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(CommandPipelineAcceptance, connection_can_be_established)
{
    this->whenConnect();

    this->thenConnectSucceeded();
    this->andConnectionIsKnownOnNode();
}

TYPED_TEST_P(
    CommandPipelineAcceptance,
    transport_connection_is_removed_before_http_response_has_been_sent)
{
    constexpr int connectionToEstablishCount = 11;

    this->setMaximumActiveConnectionCountPerSystem(1);
   
    this->establishSameSystemConnectionsSimultaneosly(connectionToEstablishCount);

    this->waitForEveryConnectToComplete();
}

REGISTER_TYPED_TEST_CASE_P(CommandPipelineAcceptance,
    connection_can_be_established,
    transport_connection_is_removed_before_http_response_has_been_sent
);

} // namespace nx::clusterdb::engine::transport::test
