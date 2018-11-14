#include "account_manager_stub.h"

namespace nx::cloud::db {
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

nx::sql::DBResult AccountManagerStub::insertAccount(
    nx::sql::QueryContext* const /*queryContext*/,
    data::AccountData /*account*/)
{
    // TODO
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountManagerStub::fetchAccountByEmail(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    auto it = m_emailToAccount.find(accountEmail);
    if (it == m_emailToAccount.end())
        return nx::sql::DBResult::notFound;

    *accountData = it->second;
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountManagerStub::createPasswordResetCode(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& /*accountEmail*/,
    std::chrono::seconds /*codeExpirationTimeout*/,
    data::AccountConfirmationCode* const /*confirmationCode*/)
{
    // TODO
    return nx::sql::DBResult::ok;
}

void AccountManagerStub::addAccount(AccountWithPassword account)
{
    auto accountEmail = account.email;
    m_emailToAccount.emplace(accountEmail, std::move(account));
}

} // namespace test
} // namespace nx::cloud::db
