#pragma once

#include <chrono>

#include <nx/utils/basic_factory.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>
#include <nx/utils/std/optional.h>

#include "../data/account_data.h"

namespace nx::cloud::db {
namespace dao {

// TODO: #ak Get rid of duplicate update and updateAccount methods.

class AbstractAccountDataObject
{
public:
    virtual ~AbstractAccountDataObject() = default;

    virtual nx::sql::DBResult insert(
        nx::sql::QueryContext* queryContext,
        const api::AccountData& account) = 0;

    virtual nx::sql::DBResult update(
        nx::sql::QueryContext* queryContext,
        const api::AccountData& account) = 0;

    virtual std::optional<api::AccountData> fetchAccountByEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail) = 0;

    virtual nx::sql::DBResult fetchAccounts(
        nx::sql::QueryContext* queryContext,
        std::vector<api::AccountData>* accounts) = 0;

    // TODO: #ak Replace QDateTime with std::chrono::steady_clock::time_point
    virtual void insertEmailVerificationCode(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        const std::string& emailVerificationCode,
        const QDateTime& codeExpirationTime) = 0;

    virtual std::optional<std::string> getVerificationCodeByAccountEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail) = 0;

    virtual nx::sql::DBResult getAccountEmailByVerificationCode(
        nx::sql::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail) = 0;

    virtual nx::sql::DBResult removeVerificationCode(
        nx::sql::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode) = 0;

    virtual nx::sql::DBResult updateAccountToActiveStatus(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime) = 0;

    virtual void updateAccount(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        const api::AccountUpdateData& accountUpdateData) = 0;
};

//-------------------------------------------------------------------------------------------------

using AccountDataObjectFactoryFunction =
    std::unique_ptr<AbstractAccountDataObject>();

class AccountDataObjectFactory:
    public nx::utils::BasicFactory<AccountDataObjectFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<AccountDataObjectFactoryFunction>;

public:
    AccountDataObjectFactory();

    static AccountDataObjectFactory& instance();

private:
    std::unique_ptr<AbstractAccountDataObject> defaultFactoryFunction();
};

} // namespace dao
} // namespace nx::cloud::db
