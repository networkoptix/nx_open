// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/abstract_remote_connection_factory_requests.h>

namespace nx::vms::client::core {
namespace test {

class RemoteConnectionFactoryRequestsManager: public AbstractRemoteConnectionFactoryRequestsManager
{
public:
    /** Number of Server API requests sent. Cloud requests are not counted. */
    int requestsCount() const { return m_requestsCount; }

    void setHandshakeCertificateChain(nx::network::ssl::CertificateChain handshakeCertificateChain)
    {
        m_handshakeCertificateChain = handshakeCertificateChain;
    }

    void addServer(nx::vms::api::ServerInformationV1 server) { m_servers.push_back(server); }

    struct UserInfo
    {
        std::string username;
        std::string password;
        std::string token;
        nx::vms::api::UserType userType = nx::vms::api::UserType::local;
    };

    /** Add user to the system. Empty token makes user support digest auth only. */
    void addUser(UserInfo user) { m_users.push_back(user); }

    virtual ModuleInformationReply getModuleInformation(ContextPtr context) const override;

    virtual ServersInfoReply getServersInfo(ContextPtr context) const override;

    virtual nx::vms::api::UserModelV1 getUserModel(ContextPtr context) const override;
    virtual nx::vms::api::UserRoleModel getUserRoleModel(ContextPtr context) const override;

    virtual nx::vms::api::LoginUser getUserType(ContextPtr context) const override;
    virtual nx::vms::api::LoginSession createLocalSession(ContextPtr context) const override;
    virtual nx::vms::api::LoginSession createTemporaryLocalSession(
        ContextPtr context) const override;

    virtual nx::vms::api::LoginSession getCurrentSession(ContextPtr context) const override;
    virtual void checkDigestAuthentication(ContextPtr context) const override;

    virtual std::future<Context::CloudTokenInfo> issueCloudToken(
        ContextPtr context,
        const QString& cloudSystemId) const override;

private:
    mutable int m_requestsCount = 0;
    nx::network::ssl::CertificateChain m_handshakeCertificateChain;
    std::vector<nx::vms::api::ServerInformationV1> m_servers;
    std::vector<UserInfo> m_users;

    struct AccessToken
    {
        std::string username;
        QString cloudSystemId;
        std::string token;
    };
    mutable std::vector<std::pair<QString, std::string>> accessTokens;
};

} // namespace test
} // namespace nx::vms::client::core
