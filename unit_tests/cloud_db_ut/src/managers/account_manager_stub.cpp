#include "account_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

boost::optional<data::AccountData> AccountManagerStub::findAccountByUserName(
    const std::string& userName) const
{
    auto it = m_emailToAccount.find(userName);
    if (it == m_emailToAccount.end())
        return boost::none;

    return data::AccountData(it->second);
}

void AccountManagerStub::addExtension(AbstractAccountManagerExtension*)
{
}

void AccountManagerStub::removeExtension(AbstractAccountManagerExtension*)
{
}

std::string AccountManagerStub::generateNewAccountId() const
{
    return std::string();
}

nx::utils::db::DBResult AccountManagerStub::insertAccount(
    nx::utils::db::QueryContext* const /*queryContext*/,
    data::AccountData /*account*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult AccountManagerStub::fetchAccountByEmail(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& /*accountEmail*/,
    data::AccountData* const /*accountData*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult AccountManagerStub::createPasswordResetCode(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& /*accountEmail*/,
    std::chrono::seconds /*codeExpirationTimeout*/,
    data::AccountConfirmationCode* const /*confirmationCode*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

void AccountManagerStub::addAccount(AccountWithPassword account)
{
    auto accountEmail = account.email;
    m_emailToAccount.emplace(accountEmail, std::move(account));
}

} // namespace test
} // namespace cdb
} // namespace nx
