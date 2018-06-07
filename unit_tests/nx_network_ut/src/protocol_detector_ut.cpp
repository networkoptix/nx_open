#include <memory>

#include <gtest/gtest.h>

#include <nx/network/protocol_detector.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace test {

class ProtocolDetector:
    public ::testing::Test
{
public:
    ProtocolDetector()
    {
        m_protocols.push_back(std::make_tuple("abc", 1));
        m_protocols.push_back(std::make_tuple("ade", 2));

        for (const auto protocol: m_protocols)
        {
            m_protocolDetector.registerProtocol(
                std::make_unique<FixedProtocolPrefixRule>(std::get<0>(protocol)),
                std::get<1>(protocol));
        }
    }

protected:
    void whenMatchBufferOfKnownProtocol()
    {
        const auto& protocolData = nx::utils::random::choice(m_protocols);
        m_expectedProtocolId = std::get<1>(protocolData);

        m_matchResult = m_protocolDetector.match(
            std::get<0>(protocolData).c_str());
    }

    void whenMatchPrefixOfKnownProtocol()
    {
        m_matchResult = m_protocolDetector.match("a");
    }

    void whenMatchLargeBufferOfUnknownProtocol()
    {
        m_matchResult = m_protocolDetector.match("wxyz");
    }

    void thenProtocolMatched()
    {
        thenMatchReported(ProtocolMatchResult::detected);
        ASSERT_EQ(m_expectedProtocolId, *std::get<1>(m_matchResult));
    }

    void thenMatchReported(ProtocolMatchResult detectionResult)
    {
        ASSERT_EQ(detectionResult, std::get<0>(m_matchResult));
    }

private:
    network::ProtocolDetector<int> m_protocolDetector;
    std::tuple<ProtocolMatchResult, int*> m_matchResult;
    std::vector<std::tuple<std::string, int>> m_protocols;
    int m_expectedProtocolId = -1;
};

TEST_F(ProtocolDetector, detects_known_protocol)
{
    whenMatchBufferOfKnownProtocol();
    thenProtocolMatched();
}

TEST_F(ProtocolDetector, requests_more_data_if_appropriate)
{
    whenMatchPrefixOfKnownProtocol();
    thenMatchReported(ProtocolMatchResult::needMoreData);
}

TEST_F(ProtocolDetector, detects_unknown_protocol)
{
    whenMatchLargeBufferOfUnknownProtocol();
    thenMatchReported(ProtocolMatchResult::unknownProtocol);
}

} // namespace test
} // namespace network
} // namespace nx
