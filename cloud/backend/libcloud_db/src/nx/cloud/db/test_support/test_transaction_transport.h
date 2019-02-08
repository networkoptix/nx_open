#pragma once

#include <nx_ec/ec_proto_version.h>
#include <transaction/transaction_transport_base.h>

namespace nx::cloud::db {
namespace test {

class CommonHttpConnection:
    public ::ec2::QnTransactionTransportBase
{
public:
    CommonHttpConnection(
        ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
        nx::vms::api::PeerData localPeer,
        const std::string& systemId,
        const std::string& systemAuthKey,
        int protocolVersion,
        int connectionId);

    int connectionId() const;

protected:
    virtual void fillAuthInfo(
        const nx::network::http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    const std::string m_systemId;
    const std::string m_systemAuthKey;
    const int m_connectionId;
};

} // namespace test
} // namespace nx::cloud::db
