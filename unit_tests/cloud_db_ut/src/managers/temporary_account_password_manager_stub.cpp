#include "temporary_account_password_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

void TemporaryAccountPasswordManagerStub::authenticateByName(
    const nx::network::http::StringType& /*username*/,
    const std::function<bool(const nx::Buffer&)>& /*validateHa1Func*/,
    nx::utils::stree::ResourceContainer* const /*authProperties*/,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> /*completionHandler*/)
{
    // TODO
}

void TemporaryAccountPasswordManagerStub::registerTemporaryCredentials(
    const AuthorizationInfo& /*authzInfo*/,
    data::TemporaryAccountCredentials /*tmpPasswordData*/,
    std::function<void(api::ResultCode)> /*completionHandler*/)
{
}

nx::utils::db::DBResult TemporaryAccountPasswordManagerStub::fetchTemporaryCredentials(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/,
    data::Credentials* /*credentials*/)
{
    return nx::utils::db::DBResult::notFound;
}

nx::utils::db::DBResult TemporaryAccountPasswordManagerStub::updateCredentialsAttributes(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const data::Credentials& /*credentials*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/)
{
    return nx::utils::db::DBResult::ioError;
}

void TemporaryAccountPasswordManagerStub::addRandomCredentials(
    const std::string& /*accountEmail*/,
    data::TemporaryAccountCredentials* const /*data*/)
{
}

nx::utils::db::DBResult TemporaryAccountPasswordManagerStub::registerTemporaryCredentials(
    nx::utils::db::QueryContext* const /*queryContext*/,
    data::TemporaryAccountCredentials tempPasswordData)
{
    m_registeredCredentials.push_back(tempPasswordData);
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult TemporaryAccountPasswordManagerStub::removeTemporaryPasswordsFromDbByAccountEmail(
    nx::utils::db::QueryContext* const /*queryContext*/,
    std::string /*accountEmail*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

void TemporaryAccountPasswordManagerStub::removeTemporaryPasswordsFromCacheByAccountEmail(
    std::string /*accountEmail*/)
{
}

boost::optional<TemporaryAccountCredentialsEx>
    TemporaryAccountPasswordManagerStub::getCredentialsByLogin(
        const std::string& /*login*/) const
{
    return boost::none;
}

bool TemporaryAccountPasswordManagerStub::authorize(
    const std::string& /*credentialsId*/,
    const nx::utils::stree::AbstractResourceReader& /*inputData*/) const
{
    // TODO
    return true;
}

} // namespace test
} // namespace cdb
} // namespace nx
