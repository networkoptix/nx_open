#pragma once

#include <vector>

#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>

namespace nx {
namespace cdb {
namespace test {

class TemporaryAccountPasswordManagerStub:
    public AbstractTemporaryAccountPasswordManager
{
public:
    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void addRandomCredentials(data::TemporaryAccountCredentials* const data) override;

    virtual nx::utils::db::DBResult registerTemporaryCredentials(
        nx::utils::db::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) override;

    virtual nx::utils::db::DBResult fetchTemporaryCredentials(
        nx::utils::db::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData,
        data::Credentials* credentials) override;

    virtual nx::utils::db::DBResult removeTemporaryPasswordsFromDbByAccountEmail(
        nx::utils::db::QueryContext* const queryContext,
        std::string accountEmail) override;

    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) override;

    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;

    const std::vector<data::TemporaryAccountCredentials>& registeredCredentials() const
    {
        return m_registeredCredentials;
    }

private:
    std::vector<data::TemporaryAccountCredentials> m_registeredCredentials;
};

} // namespace test
} // namespace cdb
} // namespace nx
