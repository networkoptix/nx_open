#pragma once

#include <nx/cloud/db/managers/account_manager.h>
#include <nx/cloud/db/test_support/business_data_generator.h>

namespace nx::cloud::db {
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

    virtual nx::sql::DBResult insertAccount(
        nx::sql::QueryContext* const queryContext,
        data::AccountData account) override;

    virtual nx::sql::DBResult fetchAccountByEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) override;

    virtual nx::sql::DBResult createPasswordResetCode(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::chrono::seconds codeExpirationTimeout,
        data::AccountConfirmationCode* const confirmationCode) override;

    void addAccount(AccountWithPassword account);

private:
    std::map<std::string, AccountWithPassword> m_emailToAccount;
};

} // namespace test
} // namespace nx::cloud::db
