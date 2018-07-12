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

nx::sql::DBResult TemporaryAccountPasswordManagerStub::fetchTemporaryCredentials(
    nx::sql::QueryContext* const /*queryContext*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/,
    data::Credentials* /*credentials*/)
{
    return nx::sql::DBResult::notFound;
}

nx::sql::DBResult TemporaryAccountPasswordManagerStub::updateCredentialsAttributes(
    nx::sql::QueryContext* const /*queryContext*/,
    const data::Credentials& /*credentials*/,
    const data::TemporaryAccountCredentials& /*tempPasswordData*/)
{
    return nx::sql::DBResult::ioError;
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

nx::sql::DBResult TemporaryAccountPasswordManagerStub::removeTemporaryPasswordsFromDbByAccountEmail(
    nx::sql::QueryContext* const /*queryContext*/,
    std::string /*accountEmail*/)
{
    // TODO
    return nx::sql::DBResult::ok;
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
