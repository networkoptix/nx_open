#include "test_transaction_transport.h"

namespace nx::cloud::db {
namespace test {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

CommonHttpConnection::CommonHttpConnection(
    ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
    nx::vms::api::PeerData localPeer,
    const std::string& systemId,
    const std::string& systemAuthKey,
    int protocolVersion,
    int connectionId)
    :
    ec2::QnTransactionTransportBase(
        QnUuid(), //< localSystemId. Not used here
        connectionGuardSharedState,
        std::move(localPeer),
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount),
    m_systemId(systemId),
    m_systemAuthKey(systemAuthKey),
    m_connectionId(connectionId)
{
    setLocalPeerProtocolVersion(protocolVersion);
}

int CommonHttpConnection::connectionId() const
{
    return m_connectionId;
}

void CommonHttpConnection::fillAuthInfo(
    const nx::network::http::AsyncHttpClientPtr& httpClient,
    bool /*authByKey*/)
{
    httpClient->setUserName(QString::fromStdString(m_systemId));
    httpClient->setUserPassword(QString::fromStdString(m_systemAuthKey));
}

} // namespace test
} // namespace nx::cloud::db
