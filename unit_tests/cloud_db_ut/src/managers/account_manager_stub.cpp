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

void AccountManagerStub::addAccount(AccountWithPassword account)
{
    m_emailToAccount.emplace(account.email, std::move(account));
}

} // namespace test
} // namespace cdb
} // namespace nx
