#pragma once

#include <chrono>
#include <map>

#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../account_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace memory {

class AccountDataObject:
    public AbstractAccountDataObject
{
public:
    virtual nx::utils::db::DBResult insert(
        nx::utils::db::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual nx::utils::db::DBResult update(
        nx::utils::db::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual nx::utils::db::DBResult fetchAccountByEmail(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) override;

    virtual nx::utils::db::DBResult fetchAccounts(
        nx::utils::db::QueryContext* queryContext,
        std::vector<data::AccountData>* accounts) override;

    virtual void insertEmailVerificationCode(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail,
        const std::string& emailVerificationCode,
        const QDateTime& codeExpirationTime) override;

    virtual boost::optional<std::string> getVerificationCodeByAccountEmail(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail) override;

    virtual nx::utils::db::DBResult getAccountEmailByVerificationCode(
        nx::utils::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail) override;

    virtual nx::utils::db::DBResult removeVerificationCode(
        nx::utils::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode) override;

    virtual nx::utils::db::DBResult updateAccountToActiveStatus(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime) override;

    virtual void updateAccount(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountEmail,
        const api::AccountUpdateData& accountUpdateData,
        bool activateAccountIfNotActive) override;

private:
    std::map<std::string, api::AccountData> m_emailToAccount;
    std::map<std::string, std::string> m_verificationCodeToEmail;
};

} // namespace memory
} // namespace dao
} // namespace cdb
} // namespace nx
