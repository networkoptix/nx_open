#include "rdb_account_data_object.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/sql/query.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

nx::sql::DBResult AccountDataObject::insert(
    nx::sql::QueryContext* queryContext,
    const api::AccountData& account)
{
    QSqlQuery insertAccountQuery(*queryContext->connection()->qtSqlConnection());
    insertAccountQuery.prepare(R"sql(
        INSERT INTO account (id, email, password_ha1, password_ha1_sha256,
            full_name, customization, status_code, registration_time_utc)
        VALUES  (:id, :email, :passwordHa1, :passwordHa1Sha256,
            :fullName, :customization, :statusCode, :registrationTime)
        )sql");
    QnSql::bind(account, &insertAccountQuery);
    if (!insertAccountQuery.exec())
    {
        NX_DEBUG(this, lm("Could not insert account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(insertAccountQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::update(
    nx::sql::QueryContext* queryContext,
    const api::AccountData& account)
{
    QSqlQuery updateAccountQuery(*queryContext->connection()->qtSqlConnection());
    updateAccountQuery.prepare(R"sql(
        UPDATE account
        SET password_ha1=:passwordHa1, password_ha1_sha256=:passwordHa1Sha256,
            full_name=:fullName, customization=:customization, status_code=:statusCode
        WHERE id=:id AND email=:email
        )sql");
    QnSql::bind(account, &updateAccountQuery);
    if (!updateAccountQuery.exec())
    {
        NX_DEBUG(this, lm("Could not update account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(updateAccountQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

std::optional<api::AccountData> AccountDataObject::fetchAccountByEmail(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail)
{
    nx::sql::SqlQuery fetchAccountQuery(queryContext->connection());
    fetchAccountQuery.setForwardOnly(true);
    fetchAccountQuery.prepare(
        R"sql(
        SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256,
               full_name as fullName, customization, status_code as statusCode
        FROM account
        WHERE email=:email
        )sql");
    fetchAccountQuery.bindValue(":email", QnSql::serialized_field(accountEmail));

    try
    {
        fetchAccountQuery.exec();
        if (!fetchAccountQuery.next())
            return std::nullopt;
    }
    catch (const nx::sql::Exception& e)
    {
        NX_DEBUG(this, lm("Error fetching account %1 from DB. %2")
            .args(accountEmail, e.what()));
        throw;
    }

    api::AccountData account;
    // Account exists.
    QnSql::fetch(
        QnSql::mapping<api::AccountData>(fetchAccountQuery.impl()),
        fetchAccountQuery.record(),
        &account);
    return account;
}

nx::sql::DBResult AccountDataObject::fetchAccounts(
    nx::sql::QueryContext* queryContext,
    std::vector<api::AccountData>* accounts)
{
    QSqlQuery readAccountsQuery(*queryContext->connection()->qtSqlConnection());
    readAccountsQuery.setForwardOnly(true);
    readAccountsQuery.prepare(R"sql(
        SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256,
            full_name as fullName, customization, status_code as statusCode,
            registration_time_utc as registrationTime, activation_time_utc as activationTime
        FROM account
        )sql");
    if (!readAccountsQuery.exec())
    {
        NX_WARNING(this, lit("Failed to read account list from DB. %1").
            arg(readAccountsQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    QnSql::fetch_many(readAccountsQuery, accounts);

    return nx::sql::DBResult::ok;
}

void AccountDataObject::insertEmailVerificationCode(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail,
    const std::string& emailVerificationCode,
    const QDateTime& codeExpirationTime)
{
    nx::sql::SqlQuery insertEmailVerificationQuery(queryContext->connection());
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
    catch (const nx::sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not insert account (%1) verification code (%2) into DB. %3")
            .arg(accountEmail).arg(emailVerificationCode).arg(e.what()));
        throw;
    }
}

std::optional<std::string> AccountDataObject::getVerificationCodeByAccountEmail(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail)
{
    nx::sql::SqlQuery fetchActivationCodesQuery(queryContext->connection());
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
    catch (const nx::sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not fetch account %1 activation codes from DB. %2").
            arg(accountEmail).arg(e.what()));
        throw;
    }

    if (!fetchActivationCodesQuery.next())
        return std::nullopt;

    return fetchActivationCodesQuery.value(lit("verification_code")).toString().toStdString();
}

nx::sql::DBResult AccountDataObject::getAccountEmailByVerificationCode(
    nx::sql::QueryContext* queryContext,
    const data::AccountConfirmationCode& verificationCode,
    std::string* accountEmail)
{
    QSqlQuery getAccountByVerificationCode(*queryContext->connection()->qtSqlConnection());
    getAccountByVerificationCode.setForwardOnly(true);
    getAccountByVerificationCode.prepare(R"sql(
        SELECT a.email
        FROM email_verification ev, account a
        WHERE ev.account_id = a.id AND ev.verification_code LIKE :code
        )sql");
    QnSql::bind(verificationCode, &getAccountByVerificationCode);
    if (!getAccountByVerificationCode.exec())
        return nx::sql::DBResult::ioError;
    if (!getAccountByVerificationCode.next())
    {
        NX_DEBUG(this, lit("Email verification code %1 was not found in the database").
            arg(QString::fromStdString(verificationCode.code)));
        return nx::sql::DBResult::notFound;
    }
    *accountEmail = QnSql::deserialized_field<QString>(
        getAccountByVerificationCode.value(0)).toStdString();

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::removeVerificationCode(
    nx::sql::QueryContext* queryContext,
    const data::AccountConfirmationCode& verificationCode)
{
    QSqlQuery removeVerificationCodeQuery(*queryContext->connection()->qtSqlConnection());
    removeVerificationCodeQuery.prepare(R"sql(
        DELETE FROM email_verification WHERE verification_code LIKE :code
        )sql");
    QnSql::bind(verificationCode, &removeVerificationCodeQuery);
    if (!removeVerificationCodeQuery.exec())
    {
        NX_DEBUG(this, lit("Failed to remove account verification code %1 from DB. %2")\
            .arg(QString::fromStdString(verificationCode.code))
            .arg(removeVerificationCodeQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountDataObject::updateAccountToActiveStatus(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail,
    std::chrono::system_clock::time_point activationTime)
{
    QSqlQuery updateAccountStatus(*queryContext->connection()->qtSqlConnection());
    updateAccountStatus.prepare(R"sql(
        UPDATE account SET status_code = ?, activation_time_utc = ? WHERE email = ?
        )sql");
    updateAccountStatus.bindValue(0, static_cast<int>(api::AccountStatus::activated));
    updateAccountStatus.bindValue(1, QnSql::serialized_field(activationTime));
    updateAccountStatus.bindValue(2, QnSql::serialized_field(accountEmail));
    if (!updateAccountStatus.exec())
    {
        NX_DEBUG(this, lm("Failed to update account %1 status. %2").
            arg(accountEmail).arg(updateAccountStatus.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

void AccountDataObject::updateAccount(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail,
    const api::AccountUpdateData& accountUpdateData)
{
    std::vector<nx::sql::SqlFilterField> fieldsToSet =
        prepareAccountFieldsToUpdate(accountUpdateData);

    NX_ASSERT(!fieldsToSet.empty());

    executeUpdateAccountQuery(
        queryContext,
        accountEmail,
        std::move(fieldsToSet));
}

std::vector<nx::sql::SqlFilterField> AccountDataObject::prepareAccountFieldsToUpdate(
    const api::AccountUpdateData& accountData)
{
    using namespace nx::sql;

    std::vector<SqlFilterField> fieldsToSet;

    if (accountData.passwordHa1)
    {
        fieldsToSet.push_back(SqlFilterFieldEqual(
            "password_ha1", ":passwordHa1",
            QnSql::serialized_field(accountData.passwordHa1.get())));
    }

    if (accountData.passwordHa1Sha256)
    {
        fieldsToSet.push_back(SqlFilterFieldEqual(
            "password_ha1_sha256", ":passwordHa1Sha256",
            QnSql::serialized_field(accountData.passwordHa1Sha256.get())));
    }

    if (accountData.fullName)
    {
        fieldsToSet.push_back(SqlFilterFieldEqual(
            "full_name", ":fullName",
            QnSql::serialized_field(accountData.fullName.get())));
    }

    if (accountData.customization)
    {
        fieldsToSet.push_back(SqlFilterFieldEqual(
            "customization", ":customization",
            QnSql::serialized_field(accountData.customization.get())));
    }

    return fieldsToSet;
}

void AccountDataObject::executeUpdateAccountQuery(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::vector<nx::sql::SqlFilterField> fieldsToSet)
{
    nx::sql::SqlQuery updateAccountQuery(queryContext->connection());
    updateAccountQuery.prepare(
        lm("UPDATE account SET %1 WHERE email=:email")
            .arg(nx::sql::joinFields(fieldsToSet, ",")));
    nx::sql::bindFields(&updateAccountQuery.impl(), fieldsToSet);
    updateAccountQuery.bindValue(
        ":email",
        QnSql::serialized_field(accountEmail));
    try
    {
        updateAccountQuery.exec();
    }
    catch (const nx::sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not update account in DB. %1").arg(e.what()));
        throw;
    }
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
