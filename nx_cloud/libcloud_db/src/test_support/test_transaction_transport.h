#pragma once

#include <nx_ec/ec_proto_version.h>
#include <transaction/transaction_transport_base.h>

namespace nx {
namespace cdb {
namespace test {

class TransactionTransport:
    public ::ec2::QnTransactionTransportBase
{
public:
    TransactionTransport(
        ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
        ::ec2::ApiPeerData localPeer,
        const std::string& systemId,
        const std::string& systemAuthKey,
        int protocolVersion = nx_ec::EC2_PROTO_VERSION);

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    const std::string m_systemId;
    const std::string m_systemAuthKey;
};

} // namespace test
} // namespace cdb
} // namespace nx
