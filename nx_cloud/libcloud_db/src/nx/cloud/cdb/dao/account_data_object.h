#pragma once

#include <chrono>

#include <boost/optional.hpp>

#include <nx/utils/basic_factory.h>

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../data/account_data.h"

namespace nx {
namespace cdb {
namespace dao {

class AbstractAccountDataObject
{
public:
    virtual ~AbstractAccountDataObject() = default;

    virtual nx::db::DBResult insert(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account) = 0;

    virtual nx::db::DBResult update(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account) = 0;

    virtual nx::db::DBResult fetchAccountByEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) = 0;

    virtual nx::db::DBResult fetchAccounts(
        nx::db::QueryContext* queryContext,
        std::vector<data::AccountData>* accounts) = 0;

    // TODO: #ak Replace QDateTime with std::chrono::steady_clock::time_point
    virtual void insertEmailVerificationCode(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        const std::string& emailVerificationCode,
        const QDateTime& codeExpirationTime) = 0;

    virtual boost::optional<std::string> getVerificationCodeByAccountEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail) = 0;

    virtual nx::db::DBResult getAccountEmailByVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail) = 0;

    virtual nx::db::DBResult removeVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode) = 0;

    virtual nx::db::DBResult updateAccountToActiveStatus(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime) = 0;
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
} // namespace cdb
} // namespace nx
