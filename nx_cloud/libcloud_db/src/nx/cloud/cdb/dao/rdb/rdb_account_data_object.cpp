#include "rdb_account_data_object.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <utils/db/query.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

nx::db::DBResult AccountDataObject::insert(
    nx::db::QueryContext* queryContext,
    const api::AccountData& account)
{
    QSqlQuery insertAccountQuery(*queryContext->connection());
    insertAccountQuery.prepare(R"sql(
        INSERT INTO account (id, email, password_ha1, password_ha1_sha256, 
            full_name, customization, status_code, registration_time_utc)
        VALUES  (:id, :email, :passwordHa1, :passwordHa1Sha256,
            :fullName, :customization, :statusCode, :registrationTime)
        )sql");
    QnSql::bind(account, &insertAccountQuery);
    if (!insertAccountQuery.exec())
    {
        NX_LOG(lm("Could not insert account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(insertAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::update(
    nx::db::QueryContext* queryContext,
    const api::AccountData& account)
{
    QSqlQuery updateAccountQuery(*queryContext->connection());
    updateAccountQuery.prepare(R"sql(
        UPDATE account 
        SET password_ha1=:passwordHa1, password_ha1_sha256=:passwordHa1Sha256, 
            full_name=:fullName, customization=:customization, status_code=:statusCode
        WHERE id=:id AND email=:email
        )sql");
    QnSql::bind(account, &updateAccountQuery);
    if (!updateAccountQuery.exec())
    {
        NX_LOG(lm("Could not update account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(updateAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::fetchAccountByEmail(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    QSqlQuery fetchAccountQuery(*queryContext->connection());
    fetchAccountQuery.setForwardOnly(true);
    fetchAccountQuery.prepare(
        R"sql(
        SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256,
               full_name as fullName, customization, status_code as statusCode
        FROM account
        WHERE email=:email
        )sql");
    fetchAccountQuery.bindValue(":email", QnSql::serialized_field(accountEmail));
    if (!fetchAccountQuery.exec())
    {
        NX_LOGX(lm("Error fetching account %1 from DB. %2")
            .arg(accountEmail).arg(fetchAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    if (!fetchAccountQuery.next())
        return nx::db::DBResult::notFound;

    // Account exists.
    QnSql::fetch(
        QnSql::mapping<data::AccountData>(fetchAccountQuery),
        fetchAccountQuery.record(),
        accountData);
    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::fetchAccounts(
    nx::db::QueryContext* queryContext,
    std::vector<data::AccountData>* accounts)
{
    QSqlQuery readAccountsQuery(*queryContext->connection());
    readAccountsQuery.setForwardOnly(true);
    readAccountsQuery.prepare(R"sql(
        SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256, 
            full_name as fullName, customization, status_code as statusCode, 
            registration_time_utc as registrationTime, activation_time_utc as activationTime 
        FROM account
        )sql");
    if (!readAccountsQuery.exec())
    {
        NX_LOG(lit("Failed to read account list from DB. %1").
            arg(readAccountsQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    QnSql::fetch_many(readAccountsQuery, accounts);

    return db::DBResult::ok;
}

void AccountDataObject::insertEmailVerificationCode(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail,
    const std::string& emailVerificationCode,
    const QDateTime& codeExpirationTime)
{
    nx::db::SqlQuery insertEmailVerificationQuery(*queryContext->connection());
    insertEmailVerificationQuery.prepare(
        "INSERT INTO email_verification( account_id, verification_code, expiration_date ) "
        "VALUES ( (SELECT id FROM account WHERE email=?), ?, ? )");
    insertEmailVerificationQuery.bindValue(
        0,
        QnSql::serialized_field(accountEmail));
    insertEmailVerificationQuery.bindValue(
        1,
        QnSql::serialized_field(emailVerificationCode));
    insertEmailVerificationQuery.bindValue(
        2,
        codeExpirationTime);
    try
    {
        insertEmailVerificationQuery.exec();
    }
    catch(nx::db::Exception e)
    {
        NX_DEBUG(this, lm("Could not insert account (%1) verification code (%2) into DB. %3")
            .arg(accountEmail).arg(emailVerificationCode).arg(e.what()));
        throw;
    }
}

boost::optional<std::string> AccountDataObject::getVerificationCodeByAccountEmail(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail)
{
    nx::db::SqlQuery fetchActivationCodesQuery(*queryContext->connection());
    fetchActivationCodesQuery.setForwardOnly(true);
    fetchActivationCodesQuery.prepare(R"sql(
        SELECT verification_code 
        FROM email_verification 
        WHERE account_id=(SELECT id FROM account WHERE email=?)
        )sql");
    fetchActivationCodesQuery.bindValue(
        0,
        QnSql::serialized_field(accountEmail));
    try
    {
        fetchActivationCodesQuery.exec();
    }
    catch (nx::db::Exception e)
    {
        NX_DEBUG(this, lm("Could not fetch account %1 activation codes from DB. %2").
            arg(accountEmail).arg(e.what()));
        throw;
    }

    if (!fetchActivationCodesQuery.next())
        return boost::none;

    return fetchActivationCodesQuery.value(lit("verification_code")).toString().toStdString();
}

nx::db::DBResult AccountDataObject::getAccountEmailByVerificationCode(
    nx::db::QueryContext* queryContext,
    const data::AccountConfirmationCode& verificationCode,
    std::string* accountEmail)
{
    QSqlQuery getAccountByVerificationCode(*queryContext->connection());
    getAccountByVerificationCode.setForwardOnly(true);
    getAccountByVerificationCode.prepare(R"sql(
        SELECT a.email 
        FROM email_verification ev, account a 
        WHERE ev.account_id = a.id AND ev.verification_code LIKE :code
        )sql");
    QnSql::bind(verificationCode, &getAccountByVerificationCode);
    if (!getAccountByVerificationCode.exec())
        return db::DBResult::ioError;
    if (!getAccountByVerificationCode.next())
    {
        NX_LOG(lit("Email verification code %1 was not found in the database").
            arg(QString::fromStdString(verificationCode.code)), cl_logDEBUG1);
        return db::DBResult::notFound;
    }
    *accountEmail = QnSql::deserialized_field<QString>(
        getAccountByVerificationCode.value(0)).toStdString();

    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::removeVerificationCode(
    nx::db::QueryContext* queryContext,
    const data::AccountConfirmationCode& verificationCode)
{
    QSqlQuery removeVerificationCodeQuery(*queryContext->connection());
    removeVerificationCodeQuery.prepare(R"sql(
        DELETE FROM email_verification WHERE verification_code LIKE :code
        )sql");
    QnSql::bind(verificationCode, &removeVerificationCodeQuery);
    if (!removeVerificationCodeQuery.exec())
    {
        NX_LOG(lit("Failed to remove account verification code %1 from DB. %2")\
            .arg(QString::fromStdString(verificationCode.code))
            .arg(removeVerificationCodeQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

nx::db::DBResult AccountDataObject::updateAccountToActiveStatus(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail,
    std::chrono::system_clock::time_point activationTime)
{
    QSqlQuery updateAccountStatus(*queryContext->connection());
    updateAccountStatus.prepare(R"sql(
        UPDATE account SET status_code = ?, activation_time_utc = ? WHERE email = ?
        )sql");
    updateAccountStatus.bindValue(0, static_cast<int>(api::AccountStatus::activated));
    updateAccountStatus.bindValue(1, QnSql::serialized_field(activationTime));
    updateAccountStatus.bindValue(2, QnSql::serialized_field(accountEmail));
    if (!updateAccountStatus.exec())
    {
        NX_LOG(lm("Failed to update account %1 status. %2").
            arg(accountEmail).arg(updateAccountStatus.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
