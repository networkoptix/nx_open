#pragma once

#include <nx/network/http/test_http_server.h>
#include <nx/cloud/db/api/system_data.h>
#include <nx/cloud/db/api/auth_provider.h>

namespace nx::cloud::storage::service::test {

class CloudDbEmulator
{
public:
    class Account
    {
        friend class CloudDbEmulator;

    public:
        Account(
            CloudDbEmulator* cloudDb,
            const network::http::Credentials& credentials);

        Account& addSystem(const std::string& systemId);

        void shareSystem(
            const std::string& systemId,
            const std::string& user,
            cloud::db::api::SystemAccessRole accessLevel);

        const network::http::Credentials& credentials() const;

    private:
        CloudDbEmulator* m_cloudDb;
        network::http::Credentials m_credentials;
        std::map<std::string, cloud::db::api::SystemAccessRole> m_systems;
    };

public:
    CloudDbEmulator();

    bool bindAndListen();

    nx::utils::Url url() const;

    Account& addAccount(const network::http::Credentials& credentials);

private:
    void registerHttpApi();

    void resolveUserDigest(
        network::http::RequestContext requestContext,
        network::http::RequestProcessedHandler completionHandler);

    void getSystemAccessLevel(
        network::http::RequestContext requestContext,
        network::http::RequestProcessedHandler completionHandler);

    std::optional<std::string/*owner*/> validateRequest(
        const network::http::RequestContext& requestContext);

    std::optional<cloud::db::api::SystemAccess> getSystemAccess(
        const std::string& user,
        const std::string& systemId);

private:
    mutable QnMutex m_mutex;
    std::map<std::string /*owner*/, Account> m_accounts;
    network::http::TestHttpServer m_server;
};

} // namespace nx::cloud::storage::service::test