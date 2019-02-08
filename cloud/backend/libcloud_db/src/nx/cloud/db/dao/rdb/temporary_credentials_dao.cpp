#include "temporary_credentials_dao.h"

#include <chrono>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/network/http/auth_tools.h>
#include <nx/sql/query.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

namespace nx::cloud::db::dao::rdb {

void TemporaryCredentialsDao::insert(
    nx::sql::QueryContext* queryContext,
    const nx::utils::stree::ResourceNameSet& attributeNameset,
    const data::TemporaryAccountCredentialsEx& tempPasswordData)
{
    try
    {
        nx::sql::SqlQuery query(*queryContext->connection()->qtSqlConnection());
        query.prepare(R"sql(
            INSERT INTO account_password (id, account_id, login, password_ha1, realm,
                expiration_timestamp_utc, prolongation_period_sec, max_use_count,
                use_count, is_email_code, access_rights)
            VALUES (:id, (SELECT id FROM account WHERE email=:accountEmail), :login,
                :passwordString, :realm, :expirationTimestampUtc, :prolongationPeriodSec,
                :maxUseCount, :useCount, :isEmailCode, :accessRights)
            )sql");
        QnSql::bind(tempPasswordData, &query.impl());
        query.bindValue(
            ":accessRights",
            QnSql::serialized_field(tempPasswordData.accessRights.toString(attributeNameset)));

        auto passwordString = preparePasswordString(
            tempPasswordData.passwordHa1,
            tempPasswordData.password);
        query.bindValue(
            ":passwordString",
            QnSql::serialized_field(passwordString));
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not insert temporary password for account %1 into DB. %2")
            .args(tempPasswordData.accountEmail, e.what()));
        throw;
    }
}

void TemporaryCredentialsDao::removeByAccountEmail(
    nx::sql::QueryContext* queryContext,
    const std::string& email)
{
    nx::sql::SqlQuery query(*queryContext->connection()->qtSqlConnection());

    try
    {
        query.prepare(
            R"sql(
            DELETE FROM account_password
            WHERE account_id=(SELECT id FROM account WHERE email=?)
            )sql");
        query.bindValue(0, QnSql::serialized_field(email));
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Failed to remove temporary passwords of account %1. %2")
            .args(email, e.what()));
        throw;
    }
}

std::optional<data::Credentials> TemporaryCredentialsDao::find(
    nx::sql::QueryContext* queryContext,
    const nx::utils::stree::ResourceNameSet& attributeNameset,
    const data::TemporaryAccountCredentials& filter)
{
    nx::sql::SqlQuery query(*queryContext->connection()->qtSqlConnection());
    try
    {
        query.prepare(R"sql(
            SELECT ap.login, ap.password_ha1 AS passwordString
            FROM account_password ap, account a
            WHERE ap.account_id=a.id AND a.email=:accountEmail
                AND max_use_count=:maxUseCount
                AND is_email_code=:isEmailCode
                AND access_rights=:accessRights
                AND prolongation_period_sec=:prolongationPeriodSec
                AND use_count=0
            )sql");
        QnSql::bind(filter, &query.impl());
        query.bindValue(
            ":accessRights",
            QnSql::serialized_field(filter.accessRights.toString(attributeNameset)));
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Error fetching temporary password for account %1. %2")
            .args(filter.accountEmail, e.what()));
        throw e;
    }

    if (!query.next())
        return std::nullopt;

    data::Credentials credentials;

    credentials.login = query.value("login").toString().toStdString();
    const auto passwordString =
        query.value("passwordString").toString().toStdString();

    std::string passwordHa1;
    parsePasswordString(passwordString, &passwordHa1, &credentials.password);
    if (credentials.password.empty()) //< True for credentials created before this code has been written.
        return std::nullopt;

    return credentials;
}

void TemporaryCredentialsDao::update(
    nx::sql::QueryContext* queryContext,
    const nx::utils::stree::ResourceNameSet& attributeNameset,
    const data::Credentials& credentials,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    try
    {
        nx::sql::SqlQuery query(*queryContext->connection()->qtSqlConnection());
        query.prepare(
            R"sql(
            UPDATE account_password SET
                expiration_timestamp_utc = :expirationTimestampUtc,
                prolongation_period_sec = :prolongationPeriodSec,
                max_use_count = :maxUseCount,
                access_rights = :accessRights,
                is_email_code = :isEmailCode,
                use_count = :useCount
            WHERE
                login = :login AND password_ha1 = :passwordString
            )sql");
        QnSql::bind(tempPasswordData, &query.impl());
        query.bindValue(
            ":accessRights",
            QnSql::serialized_field(tempPasswordData.accessRights.toString(attributeNameset)));

        const auto passwordHa1 = nx::network::http::calcHa1(
            credentials.login.c_str(),
            tempPasswordData.realm.c_str(),
            credentials.password.c_str()).toStdString();

        auto passwordString = preparePasswordString(
            passwordHa1,
            credentials.password);
        query.bindValue(":login", QnSql::serialized_field(credentials.login));
        query.bindValue(
            ":passwordString",
            QnSql::serialized_field(passwordString));
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not update temporary password for account %1. %2")
            .args(tempPasswordData.accountEmail, e.what()));
        throw;
    }
}

void TemporaryCredentialsDao::deleteById(
    nx::sql::QueryContext* queryContext,
    const std::string& id) 
{
    try
    {
        QSqlQuery query(*queryContext->connection()->qtSqlConnection());
        query.prepare("DELETE FROM account_password WHERE id=:id");
        query.bindValue(":id", QnSql::serialized_field(id));
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Could not delete temporary password %1 from DB. %2")
            .args(id, e.what()));
        throw;
    }
}

void TemporaryCredentialsDao::deleteExpiredCredentials(
    nx::sql::QueryContext* queryContext)
{
    using namespace std::chrono;

    try
    {
        nx::sql::SqlQuery query(*queryContext->connection()->qtSqlConnection());
        query.prepare(lm(R"sql(
            DELETE FROM account_password
            WHERE expiration_timestamp_utc > 0
                AND (expiration_timestamp_utc + prolongation_period_sec) < %1;
            )sql").args(duration_cast<seconds>(nx::utils::timeSinceEpoch()).count())
            .toUtf8());
        query.exec();
    }
    catch (const sql::Exception& e)
    {
        NX_DEBUG(this, lm("Error deleting expired credentials. %1").args(e.what()));
        throw;
    }
}

std::vector<data::TemporaryAccountCredentialsEx> TemporaryCredentialsDao::fetchAll(
    nx::sql::QueryContext* queryContext,
    const nx::utils::stree::ResourceNameSet& attributeNameset)
{
    try
    {
        QSqlQuery readPasswordsQuery(*queryContext->connection()->qtSqlConnection());
        readPasswordsQuery.setForwardOnly(true);
        readPasswordsQuery.prepare(R"sql(
            SELECT ap.id,
                a.email as accountEmail,
                ap.login as login,
                ap.password_ha1 as passwordHa1,
                ap.realm,
                ap.expiration_timestamp_utc as expirationTimestampUtc,
                ap.prolongation_period_sec as prolongationPeriodSec,
                ap.max_use_count as maxUseCount,
                ap.use_count as useCount,
                ap.is_email_code as isEmailCode,
                ap.access_rights as accessRightsStr
             FROM account_password ap LEFT JOIN account a ON ap.account_id=a.id
        )sql");

        readPasswordsQuery.exec();

        std::vector<data::TemporaryAccountCredentialsEx> tmpPasswords;
        QnSql::fetch_many(readPasswordsQuery, &tmpPasswords);

        for (auto& tmpPasswordData: tmpPasswords)
        {
            // TODO: #ak Currently, password_ha1 is actually password_ha1[:plain-text password].
            // Will switch to a single plain-text password field in a future version.
            std::string password;
            parsePasswordString(
                tmpPasswordData.passwordHa1,
                &tmpPasswordData.passwordHa1,
                &password);

            tmpPasswordData.accessRights.parse(
                attributeNameset,
                tmpPasswordData.accessRightsStr);
        }

        return tmpPasswords;
    }
    catch (const sql::Exception& e)
    {
        NX_WARNING(this, lm("Failed to read temporary passwords from DB. %1").arg(e.what()));
        throw;
    }
}

void TemporaryCredentialsDao::parsePasswordString(
    const std::string& passwordString,
    std::string* passwordHa1,
    std::string* password)
{
    std::vector<std::string> passwordParts;
    boost::algorithm::split(
        passwordParts,
        passwordString,
        boost::algorithm::is_any_of(":"),
        boost::algorithm::token_compress_on);
    if (passwordParts.size() >= 1)
        *passwordHa1 = passwordParts[0];
    if (passwordParts.size() >= 2)
        *password = passwordParts[1];
}

std::string TemporaryCredentialsDao::preparePasswordString(
    const std::string& passwordHa1,
    const std::string& password)
{
    std::vector<std::string> passwordParts;
    passwordParts.reserve(2);
    passwordParts.push_back(passwordHa1);
    if (!password.empty())
        passwordParts.push_back(password);
    return boost::algorithm::join(passwordParts, ":");
}

} // namespace nx::cloud::db::dao::rdb
