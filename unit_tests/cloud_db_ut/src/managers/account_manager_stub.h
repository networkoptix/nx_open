#pragma once

#include <managers/account_manager.h>
#include <test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace test {

class AccountManagerStub:
    public AbstractAccountManager
{
public:
    virtual boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const override;

    void addAccount(AccountWithPassword account);

private:
    std::map<std::string, AccountWithPassword> m_emailToAccount;
};

} // namespace test
} // namespace cdb
} // namespace nx
