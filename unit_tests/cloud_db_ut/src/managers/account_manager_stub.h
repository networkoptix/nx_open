#pragma once

#include <nx/cloud/cdb/managers/account_manager.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace test {

class AccountManagerStub:
    public AbstractAccountManager
{
public:
    virtual boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const override;
    virtual void addExtension(AbstractAccountManagerExtension*) override;
    virtual void removeExtension(AbstractAccountManagerExtension*) override;

    void addAccount(AccountWithPassword account);

private:
    std::map<std::string, AccountWithPassword> m_emailToAccount;
};

} // namespace test
} // namespace cdb
} // namespace nx
