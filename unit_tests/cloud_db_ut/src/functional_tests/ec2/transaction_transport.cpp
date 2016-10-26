
#include "transaction_transport.h"


namespace nx {
namespace cdb {
namespace test {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

TransactionTransport::TransactionTransport(
    ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
    ec2::ApiPeerData localPeer,
    const std::string& systemId,
    const std::string& systemAuthKey)
:
    ec2::QnTransactionTransportBase(
        connectionGuardSharedState,
        std::move(localPeer),
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount),
    m_systemId(systemId),
    m_systemAuthKey(systemAuthKey)
{
}

void TransactionTransport::fillAuthInfo(
    const nx_http::AsyncHttpClientPtr& httpClient,
    bool /*authByKey*/)
{
    httpClient->setUserName(QString::fromStdString(m_systemId));
    httpClient->setUserPassword(QString::fromStdString(m_systemAuthKey));
}

}   // namespace test
}   // namespace cdb
}   // namespace nx
