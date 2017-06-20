#include "dao_memory_account_data_object.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace dao {
namespace memory {

nx::db::DBResult AccountDataObject::insert(
    nx::db::QueryContext* /*queryContext*/,
    const api::AccountData& account)
{
    if (!m_emailToAccount.emplace(account.email, account).second)
        return nx::db::DBResult::uniqueConstraintViolation;

    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::update(
    nx::db::QueryContext* /*queryContext*/,
    const api::AccountData& account)
{
    m_emailToAccount[account.email] = account;
    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::fetchAccountByEmail(
    nx::db::QueryContext* /*queryContext*/,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    auto it = m_emailToAccount.find(accountEmail);
    if (it == m_emailToAccount.end())
        return db::DBResult::notFound;

    *accountData = it->second;
    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::fetchAccounts(
    nx::db::QueryContext* /*queryContext*/,
    std::vector<data::AccountData>* /*accounts*/)
{
    // TODO
    return db::DBResult::ok;
}

void AccountDataObject::insertEmailVerificationCode(
    nx::db::QueryContext* /*queryContext*/,
    const std::string& accountEmail,
    const std::string& emailVerificationCode,
    const QDateTime& /*codeExpirationTime*/)
{
    m_verificationCodeToEmail[emailVerificationCode] = accountEmail;
}

boost::optional<std::string> AccountDataObject::getVerificationCodeByAccountEmail(
    nx::db::QueryContext* /*queryContext*/,
    const std::string& accountEmail)
{
    for (const auto& val: m_verificationCodeToEmail)
    {
        if (val.second == accountEmail)
            return val.first;
    }

    return boost::none;
}

nx::db::DBResult AccountDataObject::getAccountEmailByVerificationCode(
    nx::db::QueryContext* /*queryContext*/,
    const data::AccountConfirmationCode& verificationCode,
    std::string* accountEmail)
{
    auto it = m_verificationCodeToEmail.find(verificationCode.code);
    if (it == m_verificationCodeToEmail.end())
        return nx::db::DBResult::notFound;

    *accountEmail = it->second;
    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::removeVerificationCode(
    nx::db::QueryContext* /*queryContext*/,
    const data::AccountConfirmationCode& verificationCode)
{
    m_verificationCodeToEmail.erase(verificationCode.code);
    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::updateAccountToActiveStatus(
    nx::db::QueryContext* /*queryContext*/,
    const std::string& /*accountEmail*/,
    std::chrono::system_clock::time_point /*activationTime*/)
{
    // TODO
    return db::DBResult::ok;
}

void AccountDataObject::updateAccount(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail,
    const api::AccountUpdateData& accountUpdateData,
    bool activateAccountIfNotActive)
{
    auto accountIter = m_emailToAccount.find(accountEmail);
    if (accountIter == m_emailToAccount.end())
        throw nx::db::Exception(nx::db::DBResult::notFound);

    if (accountUpdateData.passwordHa1)
        accountIter->second.passwordHa1 = *accountUpdateData.passwordHa1;
    if (accountUpdateData.passwordHa1Sha256)
        accountIter->second.passwordHa1Sha256 = *accountUpdateData.passwordHa1Sha256;
    if (accountUpdateData.customization)
        accountIter->second.customization = *accountUpdateData.customization;
    if (accountUpdateData.fullName)
        accountIter->second.fullName = *accountUpdateData.fullName;
}

} // namespace memory
} // namespace dao
} // namespace cdb
} // namespace nx
