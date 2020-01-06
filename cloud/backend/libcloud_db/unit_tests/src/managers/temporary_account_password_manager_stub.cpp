#include "temporary_account_password_manager_stub.h"

namespace nx::cloud::db {
namespace test {

void TemporaryAccountPasswordManagerStub::authenticateByName(
    const nx::network::http::StringType& /*username*/,
    const std::function<bool(const nx::Buffer&)>& /*validateHa1Func*/,
    nx::utils::stree::ResourceContainer* const /*authProperties*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> /*completionHandler*/)
{
    // TODO
}

void TemporaryAccountPasswordManagerStub::registerTemporaryCredentials(
    const AuthorizationInfo& /*authzInfo*/,
    data::TemporaryAccountCredentials /*tmpPasswordData*/,
    std::function<void(api::Result)> /*completionHandler*/)
{
}

std::optional<data::Credentials> TemporaryAccountPasswordManagerStub::fetchTemporaryCredentials(
    nx::sql::QueryContext* const /*queryContext*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/)
{
    return std::nullopt;
}

void TemporaryAccountPasswordManagerStub::updateCredentialsAttributes(
    nx::sql::QueryContext* const /*queryContext*/,
    const data::Credentials& /*credentials*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/)
{
}

void TemporaryAccountPasswordManagerStub::addRandomCredentials(
    const std::string& /*accountEmail*/,
    data::TemporaryAccountCredentials* const /*data*/)
{
}

nx::sql::DBResult TemporaryAccountPasswordManagerStub::registerTemporaryCredentials(
    nx::sql::QueryContext* const /*queryContext*/,
    data::TemporaryAccountCredentials tempPasswordData)
{
    m_registeredCredentials.push_back(tempPasswordData);
    return nx::sql::DBResult::ok;
}

void TemporaryAccountPasswordManagerStub::removeTemporaryPasswordsFromDbByAccountEmail(
    nx::sql::QueryContext* const /*queryContext*/,
    std::string /*accountEmail*/)
{
}

void TemporaryAccountPasswordManagerStub::removeTemporaryPasswordsFromCacheByAccountEmail(
    std::string /*accountEmail*/)
{
}

std::optional<data::TemporaryAccountCredentialsEx>
    TemporaryAccountPasswordManagerStub::getCredentialsByLogin(
        const std::string& /*login*/) const
{
    return std::nullopt;
}

bool TemporaryAccountPasswordManagerStub::authorize(
    const std::string& /*credentialsId*/,
    const nx::utils::stree::AbstractResourceReader& /*inputData*/) const
{
    // TODO
    return true;
}

} // namespace test
} // namespace nx::cloud::db
