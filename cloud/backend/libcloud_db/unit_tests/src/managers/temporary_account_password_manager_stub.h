#pragma once

#include <vector>

#include <nx/cloud/db/managers/temporary_account_password_manager.h>

namespace nx::cloud::db {
namespace test {

class TemporaryAccountPasswordManagerStub:
    public AbstractTemporaryAccountPasswordManager
{
public:
    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        const std::function<bool(const nx::Buffer&)>& validateHa1Func,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::Result)> completionHandler) override;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::Result)> completionHandler) override;

    virtual void addRandomCredentials(
        const std::string& /*accountEmail*/,
        data::TemporaryAccountCredentials* const data) override;

    virtual nx::sql::DBResult registerTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) override;

    virtual std::optional<data::Credentials> fetchTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual void updateCredentialsAttributes(
        nx::sql::QueryContext* const queryContext,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual void removeTemporaryPasswordsFromDbByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        std::string accountEmail) override;

    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) override;

    virtual std::optional<data::TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;

    virtual bool authorize(
        const std::string& credentialsId,
        const nx::utils::stree::AbstractResourceReader& inputData) const override;

    const std::vector<data::TemporaryAccountCredentials>& registeredCredentials() const
    {
        return m_registeredCredentials;
    }

private:
    std::vector<data::TemporaryAccountCredentials> m_registeredCredentials;
};

} // namespace test
} // namespace nx::cloud::db
