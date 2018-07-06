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

    virtual std::string generateNewAccountId() const override;

    virtual nx::utils::db::DBResult insertAccount(
        nx::utils::db::QueryContext* const queryContext,
        data::AccountData account) override;

    virtual nx::utils::db::DBResult fetchAccountByEmail(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) override;

    virtual nx::utils::db::DBResult createPasswordResetCode(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::chrono::seconds codeExpirationTimeout,
        data::AccountConfirmationCode* const confirmationCode) override;

    void addAccount(AccountWithPassword account);

private:
    std::map<std::string, AccountWithPassword> m_emailToAccount;
};

} // namespace test
} // namespace cdb
} // namespace nx
