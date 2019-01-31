#include "dao_memory_account_data_object.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx::cloud::db {
namespace dao {
namespace memory {

nx::sql::DBResult AccountDataObject::insert(
    nx::sql::QueryContext* /*queryContext*/,
    const api::AccountData& account)
{
    if (!m_emailToAccount.emplace(account.email, account).second)
        return nx::sql::DBResult::uniqueConstraintViolation;

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::update(
    nx::sql::QueryContext* /*queryContext*/,
    const api::AccountData& account)
{
    m_emailToAccount[account.email] = account;
    return nx::sql::DBResult::ok;
}

std::optional<api::AccountData> AccountDataObject::fetchAccountByEmail(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& accountEmail)
{
    auto it = m_emailToAccount.find(accountEmail);
    if (it == m_emailToAccount.end())
        return std::nullopt;

    return it->second;
}

nx::sql::DBResult AccountDataObject::fetchAccounts(
    nx::sql::QueryContext* /*queryContext*/,
    std::vector<api::AccountData>* /*accounts*/)
{
    // TODO
    return nx::sql::DBResult::ok;
}

void AccountDataObject::insertEmailVerificationCode(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& accountEmail,
    const std::string& emailVerificationCode,
    const QDateTime& /*codeExpirationTime*/)
{
    m_verificationCodeToEmail[emailVerificationCode] = accountEmail;
}

std::optional<std::string> AccountDataObject::getVerificationCodeByAccountEmail(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& accountEmail)
{
    for (const auto& val: m_verificationCodeToEmail)
    {
        if (val.second == accountEmail)
            return val.first;
    }

    return std::nullopt;
}

nx::sql::DBResult AccountDataObject::getAccountEmailByVerificationCode(
    nx::sql::QueryContext* /*queryContext*/,
    const data::AccountConfirmationCode& verificationCode,
    std::string* accountEmail)
{
    auto it = m_verificationCodeToEmail.find(verificationCode.code);
    if (it == m_verificationCodeToEmail.end())
        return nx::sql::DBResult::notFound;

    *accountEmail = it->second;
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::removeVerificationCode(
    nx::sql::QueryContext* /*queryContext*/,
    const data::AccountConfirmationCode& verificationCode)
{
    m_verificationCodeToEmail.erase(verificationCode.code);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::updateAccountToActiveStatus(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& /*accountEmail*/,
    std::chrono::system_clock::time_point /*activationTime*/)
{
    // TODO
    return nx::sql::DBResult::ok;
}

void AccountDataObject::updateAccount(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& accountEmail,
    const api::AccountUpdateData& accountUpdateData)
{
    auto accountIter = m_emailToAccount.find(accountEmail);
    if (accountIter == m_emailToAccount.end())
        throw nx::sql::Exception(nx::sql::DBResult::notFound);

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
} // namespace nx::cloud::db
