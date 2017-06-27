#pragma once

#include <chrono>

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../account_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class AccountDataObject:
    public AbstractAccountDataObject
{
public:
    virtual nx::db::DBResult insert(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual nx::db::DBResult update(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual nx::db::DBResult fetchAccountByEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) override;

    virtual nx::db::DBResult fetchAccounts(
        nx::db::QueryContext* queryContext,
        std::vector<data::AccountData>* accounts) override;

    virtual void insertEmailVerificationCode(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        const std::string& emailVerificationCode,
        const QDateTime& codeExpirationTime) override;

    virtual boost::optional<std::string> getVerificationCodeByAccountEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail) override;

    virtual nx::db::DBResult getAccountEmailByVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail) override;

    virtual nx::db::DBResult removeVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode) override;

    virtual nx::db::DBResult updateAccountToActiveStatus(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime) override;

    virtual void updateAccount(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        const api::AccountUpdateData& accountUpdateData,
        bool activateAccountIfNotActive) override;

private:
    std::vector<nx::db::SqlFilterField> prepareAccountFieldsToUpdate(
        const api::AccountUpdateData& accountData,
        bool activateAccountIfNotActive);
    void executeUpdateAccountQuery(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::vector<nx::db::SqlFilterField> fieldsToSet);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
