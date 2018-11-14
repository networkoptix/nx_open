#pragma once

#include <chrono>

#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../account_data_object.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class AccountDataObject:
    public AbstractAccountDataObject
{
public:
    virtual nx::sql::DBResult insert(
        nx::sql::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual nx::sql::DBResult update(
        nx::sql::QueryContext* queryContext,
        const api::AccountData& account) override;

    virtual std::optional<api::AccountData> fetchAccountByEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail) override;

    virtual nx::sql::DBResult fetchAccounts(
        nx::sql::QueryContext* queryContext,
        std::vector<api::AccountData>* accounts) override;

    virtual void insertEmailVerificationCode(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        const std::string& emailVerificationCode,
        const QDateTime& codeExpirationTime) override;

    virtual std::optional<std::string> getVerificationCodeByAccountEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail) override;

    virtual nx::sql::DBResult getAccountEmailByVerificationCode(
        nx::sql::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail) override;

    virtual nx::sql::DBResult removeVerificationCode(
        nx::sql::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode) override;

    virtual nx::sql::DBResult updateAccountToActiveStatus(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime) override;

    virtual void updateAccount(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        const api::AccountUpdateData& accountUpdateData) override;

private:
    std::vector<nx::sql::SqlFilterField> prepareAccountFieldsToUpdate(
        const api::AccountUpdateData& accountData);

    void executeUpdateAccountQuery(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::vector<nx::sql::SqlFilterField> fieldsToSet);
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
