#include "account_manager.h"

#include <chrono>
#include <functional>
#include <iostream>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/client/data/types.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>
#include <nx/utils/app_info.h>
#include <nx/utils/scope_guard.h>

#include "email_manager.h"
#include "notification.h"
#include "temporary_account_password_manager.h"
#include "../access_control/authentication_manager.h"
#include "../settings.h"
#include "../stree/cdb_ns.h"
#include "../stree/stree_manager.h"

namespace nx::cloud::db {

AccountManager::AccountManager(
    const conf::Settings& settings,
    const StreeManager& streeManager,
    AbstractTemporaryAccountPasswordManager* const tempPasswordManager,
    nx::sql::AsyncSqlQueryExecutor* const dbManager,
    AbstractEmailManager* const emailManager) noexcept(false)
:
    m_settings(settings),
    m_streeManager(streeManager),
    m_tempPasswordManager(tempPasswordManager),
    m_dbManager(dbManager),
    m_emailManager(emailManager),
    m_dao(dao::AccountDataObjectFactory::instance().create())
{
    if( fillCache() != nx::sql::DBResult::ok )
        throw std::runtime_error( "Failed to fill account cache" );
}

AccountManager::~AccountManager()
{
    //assuming no public methods of this class can be called anymore,
    //  but we have to wait for all scheduled async calls to finish
    m_startedAsyncCallsCounter.wait();
}

void AccountManager::authenticateByName(
    const nx::network::http::StringType& username,
    const std::function<bool(const nx::Buffer&)>& validateHa1Func,
    nx::utils::stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::Result)> completionHandler)
{
    api::ResultCode accountAuthResult = api::ResultCode::notAuthorized;

    {
        QnMutexLocker lk(&m_mutex);

        const auto account = m_cache.find(toStdString(username));
        if (!account)
        {
            accountAuthResult = api::ResultCode::badUsername;
        }
        else if (validateHa1Func(account->passwordHa1.c_str()))
        {
            authProperties->put(
                nx::cloud::db::attr::authAccountEmail,
                username);
            completionHandler(api::ResultCode::ok);
            return;
        }
    }

    // NOTE: Currently, every authenticateByName is synchronous.
    // Changing it to async requires some HTTP authentication refactoring.

    // Authenticating by temporary password.
    m_tempPasswordManager->authenticateByName(
        username,
        std::move(validateHa1Func),
        authProperties,
        [this, username, authProperties, accountAuthResult,
            completionHandler = std::move(completionHandler)](
                api::Result tempPwdAuthResult) mutable
        {
            if (tempPwdAuthResult == api::ResultCode::ok)
            {
                const auto authenticatedByEmailCode =
                    authProperties->get<bool>(attr::authenticatedByEmailCode);
                const auto accountEmail =
                    authProperties->get<std::string>(nx::cloud::db::attr::authAccountEmail);

                if (static_cast<bool>(authenticatedByEmailCode) && *authenticatedByEmailCode &&
                    static_cast<bool>(accountEmail))
                {
                    //activating account
                    m_cache.atomicUpdate(
                        *accountEmail,
                        [](api::AccountData& account) {
                            account.statusCode = api::AccountStatus::activated;
                        });
                }
            }

            if (tempPwdAuthResult == api::ResultCode::badUsername)
            {
                return completionHandler(
                    accountAuthResult == api::ResultCode::badUsername
                    ? api::ResultCode::badUsername
                    : api::ResultCode::notAuthorized);
            }

            completionHandler(tempPwdAuthResult);
        });
}

void AccountManager::registerAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountRegistrationData accountRegistrationData,
    std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler)
{
    data::AccountData account(std::move(accountRegistrationData));
    account.statusCode = api::AccountStatus::awaitingActivation;

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountData, data::AccountConfirmationCode>(
        std::bind(&AccountManager::registerNewAccountInDb, this, _1, _2, _3),
        std::move(account),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler),
            requestSourceSecured](
                nx::sql::DBResult dbResult,
                data::AccountData /*account*/,
                data::AccountConfirmationCode confirmationCode)
        {
            completionHandler(
                dbResultToApiResult(dbResult),
                requestSourceSecured
                    ? std::move(confirmationCode)
                    : data::AccountConfirmationCode());
        });
}

void AccountManager::activate(
    const AuthorizationInfo& /*authzInfo*/,
    data::AccountConfirmationCode emailVerificationCode,
    std::function<void(api::Result, api::AccountEmail)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountConfirmationCode, std::string>(
        std::bind(&AccountManager::verifyAccount, this, _1, _2, _3),
        std::move(emailVerificationCode),
        std::bind(&AccountManager::sendActivateAccountResponse, this,
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, std::move(completionHandler)));
}

void AccountManager::getAccount(
    const AuthorizationInfo& authzInfo,
    std::function<void(api::Result, data::AccountData)> completionHandler )
{
    QString accountEmail;
    if (!authzInfo.get( attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::forbidden, data::AccountData());
        return;
    }

    if (auto accountData = m_cache.find(accountEmail.toStdString()))
    {
        accountData.get().passwordHa1.clear();
        completionHandler(api::ResultCode::ok, std::move(accountData.get()));
        return;
    }

    // Very strange. Account has been authenticated, but not found in cache.
    completionHandler(api::ResultCode::notFound, data::AccountData());
}

void AccountManager::updateAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountUpdateData accountData,
    std::function<void(api::Result)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::forbidden);
        return;
    }
    bool authenticatedByEmailCode = false;
    authzInfo.get(attr::authenticatedByEmailCode, &authenticatedByEmailCode);

    data::AccountUpdateDataWithEmail updateDataWithEmail(std::move(accountData));
    updateDataWithEmail.email = accountEmail;

    auto onUpdateCompletion =
        [this,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            authenticatedByEmailCode,
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult resultCode,
                data::AccountUpdateDataWithEmail accountData)
        {
            if (resultCode == nx::sql::DBResult::ok)
                updateAccountCache(std::move(accountData));
            completionHandler(dbResultToApiResult(resultCode));
        };

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountUpdateDataWithEmail>(
        std::bind(&AccountManager::updateAccountInDb, this, authenticatedByEmailCode, _1, _2),
        std::move(updateDataWithEmail),
        std::move(onUpdateCompletion));
}

void AccountManager::resetPassword(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler)
{
    using namespace std::placeholders;

    auto account = m_cache.find(accountEmail.email);
    if (!account)
    {
        NX_DEBUG(this, lm("%1 (%2). Account not found").
            arg(kAccountPasswordResetPath).arg(accountEmail.email));
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    bool hasRequestCameFromSecureSource = false;
    authzInfo.get(attr::secureSource, &hasRequestCameFromSecureSource);

    m_dbManager->executeUpdate<std::string, data::AccountConfirmationCode>(
        std::bind(&AccountManager::issueRestorePasswordCode, this, _1, _2, _3),
        accountEmail.email,
        [this, hasRequestCameFromSecureSource,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResultCode,
                std::string /*accountEmail*/,
                data::AccountConfirmationCode confirmationCode)
        {
            return completionHandler(
                dbResultToApiResult(dbResultCode),
                hasRequestCameFromSecureSource
                    ? std::move(confirmationCode)
                    : data::AccountConfirmationCode());
        });
}

void AccountManager::reactivateAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler)
{
    const auto existingAccount = m_cache.find(accountEmail.email);
    if (!existingAccount)
    {
        NX_DEBUG(this, lm("/account/reactivate. Unknown account %1").
            arg(accountEmail.email));
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    if (existingAccount->statusCode == api::AccountStatus::activated)
    {
        NX_DEBUG(this, lm("/account/reactivate. Not reactivating of already activated account %1").
            arg(accountEmail.email));
        return completionHandler(
            api::ResultCode::forbidden,
            data::AccountConfirmationCode());
    }

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    auto notification = std::make_unique<ActivateAccountNotification>();
    notification->customization = existingAccount->customization;

    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string, data::AccountConfirmationCode>(
        [this, notification = std::move(notification), account = *existingAccount](
            nx::sql::QueryContext* const queryContext,
            const std::string& /*accountEmail*/,
            data::AccountConfirmationCode* const resultData) mutable
        {
            return issueAccountActivationCode(
                queryContext,
                account,
                std::move(notification),
                resultData);
        },
        std::move(accountEmail.email),
        std::bind(&AccountManager::accountReactivated, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            requestSourceSecured,
            _1, _2, _3, std::move(completionHandler)));
}

void AccountManager::createTemporaryCredentials(
    const AuthorizationInfo& authzInfo,
    data::TemporaryCredentialsParams params,
    std::function<void(api::Result, api::TemporaryCredentials)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        NX_ASSERT(false);
        return completionHandler(
            api::ResultCode::forbidden,
            api::TemporaryCredentials());
    }

    auto account = m_cache.find(accountEmail);
    if (!account)
    {
        //this can happen due to some race
        NX_DEBUG(this, lm("%1 (%2). Account not found").
            arg(kAccountCreateTemporaryCredentialsPath).arg(accountEmail));
        return completionHandler(
            api::ResultCode::notFound,
            api::TemporaryCredentials());
    }

    if (!params.type.empty())
    {
        //fetching timeouts by type
        params.timeouts = api::TemporaryCredentialsTimeouts();
        m_streeManager.search(
            StreeOperation::getTemporaryCredentialsParameters,
            nx::utils::stree::SingleResourceContainer(
                attr::credentialsType, QString::fromStdString(params.type)),
            &params);
        if (params.timeouts.expirationPeriod == std::chrono::seconds::zero())
            return completionHandler(
                api::ResultCode::badRequest,
                api::TemporaryCredentials());
    }

    //generating temporary password
    data::TemporaryAccountCredentials tempPasswordData;
    //temporary login is generated by addRandomCredentials
    tempPasswordData.accountEmail = accountEmail;
    tempPasswordData.realm = AuthenticationManager::realm().constData();
    tempPasswordData.expirationTimestampUtc =
        nx::utils::timeSinceEpoch().count() +
        params.timeouts.expirationPeriod.count();
    if (params.timeouts.autoProlongationEnabled)
    {
        tempPasswordData.prolongationPeriodSec =
            std::chrono::duration_cast<std::chrono::seconds>(
                params.timeouts.prolongationPeriod).count();
    }
    tempPasswordData.maxUseCount = std::numeric_limits<int>::max();
    //filling in access rights
    tempPasswordData.accessRights.requestsDenied.add(
        data::ApiRequestRule(
            kAccountUpdatePath,
            {{attr::ha1, true}, {attr::userPassword, true}}));
    tempPasswordData.accessRights.requestsDenied.add(kAccountPasswordResetPath);
    m_tempPasswordManager->addRandomCredentials(
        accountEmail,
        &tempPasswordData);

    api::TemporaryCredentials temporaryCredentials;
    temporaryCredentials.login = tempPasswordData.login;
    temporaryCredentials.password = tempPasswordData.password;
    temporaryCredentials.timeouts = params.timeouts;

    //adding temporary password
    m_tempPasswordManager->registerTemporaryCredentials(
        authzInfo,
        std::move(tempPasswordData),
        std::bind(&AccountManager::temporaryCredentialsSaved, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            std::placeholders::_1,
            std::move(accountEmail),
            std::move(temporaryCredentials),
            std::move(completionHandler)));
}

std::string AccountManager::generateNewAccountId() const
{
    return QnUuid::createUuid().toSimpleString().toStdString();
}

boost::optional<data::AccountData> AccountManager::findAccountByUserName(
    const std::string& userName ) const
{
    return m_cache.find(userName);
}

nx::sql::DBResult AccountManager::insertAccount(
    nx::sql::QueryContext* const queryContext,
    data::AccountData account)
{
    if (account.id.empty())
        account.id = generateNewAccountId();
    if (account.customization.empty())
        account.customization = nx::utils::AppInfo::customizationName().toStdString();

    account.registrationTime = nx::utils::utcTime();

    const auto dbResult = m_dao->insert(queryContext, account);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, account = std::move(account)]()
        {
            auto email = account.email;
            m_cache.insert(std::move(email), std::move(account));
        });

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountManager::updateAccount(
    nx::sql::QueryContext* const queryContext,
    data::AccountData account)
{
    NX_ASSERT(!account.id.empty() && !account.email.empty());
    if (account.id.empty() || account.email.empty())
        return nx::sql::DBResult::ioError;

    auto dbResult = m_dao->update(queryContext, account);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, account = std::move(account)]() mutable
        {
            auto email = account.email;
            m_cache.atomicUpdate(
                email,
                [newAccount = std::move(account)](data::AccountData& account) mutable
                {
                    account = std::move(newAccount);
                });
        });

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountManager::fetchAccountByEmail(
    nx::sql::QueryContext* queryContext,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    try
    {
        auto account = m_dao->fetchAccountByEmail(
            queryContext,
            accountEmail);
        if (!account)
            return nx::sql::DBResult::notFound;

        *accountData = *account;
        return nx::sql::DBResult::ok;
    }
    catch (const nx::sql::Exception& e)
    {
        return e.dbResult();
    }
}

nx::sql::DBResult AccountManager::createPasswordResetCode(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::chrono::seconds codeExpirationTimeout,
    data::AccountConfirmationCode* const confirmationCode)
{
    //generating temporary password
    data::TemporaryAccountCredentials tempPasswordData;
    tempPasswordData.accountEmail = accountEmail;
    tempPasswordData.login = accountEmail;
    tempPasswordData.realm = AuthenticationManager::realm().constData();
    tempPasswordData.expirationTimestampUtc =
        nx::utils::timeSinceEpoch().count() + codeExpirationTimeout.count();
    tempPasswordData.maxUseCount = 1;
    tempPasswordData.isEmailCode = true;
    tempPasswordData.accessRights.requestsAllowed.add(kAccountUpdatePath);

    const auto credentials = m_tempPasswordManager->fetchTemporaryCredentials(
        queryContext,
        tempPasswordData);

    std::string temporaryPassword;
    if (credentials)
    {
        NX_VERBOSE(this, lm("Found existing password reset code (%1) for account %2")
            .args(credentials->password, accountEmail));
        temporaryPassword = credentials->password;

        // Updating expiration time.
        m_tempPasswordManager->updateCredentialsAttributes(
            queryContext,
            *credentials,
            tempPasswordData);
    }
    else
    {
        m_tempPasswordManager->addRandomCredentials(accountEmail, &tempPasswordData);
        temporaryPassword = tempPasswordData.password;

        const auto dbResultCode = m_tempPasswordManager->registerTemporaryCredentials(
            queryContext,
            std::move(tempPasswordData));
        if (dbResultCode != nx::sql::DBResult::ok)
            return dbResultCode;
    }

    // Preparing confirmation code.
    auto resetCodeStr = temporaryPassword + ":" + accountEmail;
    confirmationCode->code = QByteArray::fromRawData(
        resetCodeStr.data(), (int)resetCodeStr.size()).toBase64().toStdString();

    return nx::sql::DBResult::ok;
}

void AccountManager::addExtension(AbstractAccountManagerExtension* extension)
{
    m_extensions.add(extension);
}

void AccountManager::removeExtension(AbstractAccountManagerExtension* extension)
{
    m_extensions.remove(extension);
}

nx::sql::DBResult AccountManager::fillCache()
{
    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect(
        std::bind(&AccountManager::fetchAccounts, this, _1),
        [&cacheFilledPromise](nx::sql::DBResult dbResult)
        {
            cacheFilledPromise.set_value( dbResult );
        });

    //waiting for completion
    future.wait();
    return future.get();
}

nx::sql::DBResult AccountManager::fetchAccounts(
    nx::sql::QueryContext* queryContext)
{
    NX_VERBOSE(this, lm("Filling account cache"));

    std::vector<api::AccountData> accounts;
    auto dbResult = m_dao->fetchAccounts(queryContext, &accounts);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    NX_VERBOSE(this, lm("Read %1 accounts").args(accounts.size()));

    for (auto& account: accounts)
    {
        auto emailCopy = account.email;
        if (!m_cache.insert(std::move(emailCopy), std::move(account)))
        {
            NX_ASSERT(false);
        }
    }

    NX_VERBOSE(this, lm("Account cache filled up"));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult AccountManager::registerNewAccountInDb(
    nx::sql::QueryContext* const queryContext,
    const data::AccountData& accountData,
    data::AccountConfirmationCode* const confirmationCode)
{
    auto existingAccount = m_dao->fetchAccountByEmail(
        queryContext,
        accountData.email);

    if (existingAccount)
    {
        if (existingAccount->statusCode != api::AccountStatus::invited)
        {
            NX_DEBUG(this, lm("Failed to add account with already used email %1, status %2").
                arg(accountData.email).arg(QnLexical::serialized(existingAccount->statusCode)));
            return nx::sql::DBResult::uniqueConstraintViolation;
        }

        existingAccount->statusCode = api::AccountStatus::awaitingActivation;
        // Merging existing account with new one.
        auto accountIdBak = std::move(existingAccount->id);
        existingAccount = accountData;
        existingAccount->id = std::move(accountIdBak);

        auto dbResult = updateAccount(queryContext, std::move(*existingAccount));
        if (dbResult != nx::sql::DBResult::ok)
        {
            NX_DEBUG(this, lm("Failed to update existing account %1")
                .arg(accountData.email));
            return dbResult;
        }
    }
    else //< !existingAccount
    {
        auto dbResult = insertAccount(queryContext, accountData);
        if (dbResult != nx::sql::DBResult::ok)
        {
            NX_DEBUG(this, lm("Failed to insert new account. Email %1")
                .arg(accountData.email));
            return dbResult;
        }
    }

    auto notification = std::make_unique<ActivateAccountNotification>();
    notification->customization = accountData.customization;
    return issueAccountActivationCode(
        queryContext,
        accountData,
        std::move(notification),
        confirmationCode);
}

nx::sql::DBResult AccountManager::issueAccountActivationCode(
    nx::sql::QueryContext* const queryContext,
    const data::AccountData& account,
    std::unique_ptr<AbstractActivateAccountNotification> notification,
    data::AccountConfirmationCode* const resultData)
{
    auto verificationCode = m_dao->getVerificationCodeByAccountEmail(
        queryContext,
        account.email);
    if (verificationCode)
    {
        resultData->code = *verificationCode;
    }
    else
    {
        const auto codeExpirationTime =
            nx::utils::timeSinceEpoch() +
                m_settings.accountManager().accountActivationCodeExpirationTimeout;
        const auto emailVerificationCode =
            generateAccountActivationCode(queryContext, account.email, codeExpirationTime);

        m_dao->insertEmailVerificationCode(
            queryContext,
            account.email,
            emailVerificationCode,
            QDateTime::fromTime_t(codeExpirationTime.count()));

        resultData->code = std::move(emailVerificationCode);
    }

    notification->setActivationCode(resultData->code);
    notification->setFullName(account.fullName);
    notification->setAddressee(account.email);
    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, notification = std::move(notification)]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });

    return nx::sql::DBResult::ok;
}

std::string AccountManager::generateAccountActivationCode(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    const std::chrono::seconds& codeExpirationTime)
{
    data::TemporaryAccountCredentials tempPasswordData;
    tempPasswordData.accountEmail = accountEmail;
    tempPasswordData.login = accountEmail;
    tempPasswordData.realm = AuthenticationManager::realm().constData();
    tempPasswordData.expirationTimestampUtc = codeExpirationTime.count();
    tempPasswordData.maxUseCount = 1;
    tempPasswordData.isEmailCode = true;
    tempPasswordData.accessRights.requestsAllowed.add(kAccountActivatePath);
    tempPasswordData.accessRights.requestsAllowed.add(kAccountUpdatePath);
    m_tempPasswordManager->addRandomCredentials(
        accountEmail,
        &tempPasswordData);
    const auto dbResult = m_tempPasswordManager->registerTemporaryCredentials(
        queryContext, tempPasswordData);
    if (dbResult != nx::sql::DBResult::ok)
        throw nx::sql::Exception(dbResult);

    return QByteArray::fromStdString(tempPasswordData.password + ":" + tempPasswordData.login)
        .toBase64().toStdString();
}

void AccountManager::accountReactivated(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    bool requestSourceSecured,
    nx::sql::DBResult resultCode,
    std::string /*email*/,
    data::AccountConfirmationCode resultData,
    std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler)
{
    //adding activation code only in response to portal
    completionHandler(
        resultCode == nx::sql::DBResult::ok
            ? api::ResultCode::ok
            : api::ResultCode::dbError,
        requestSourceSecured
            ? std::move(resultData)
            : data::AccountConfirmationCode());
}

void AccountManager::activateAccountInCache(
    std::string accountEmail,
    std::chrono::system_clock::time_point activationTime)
{
    m_cache.atomicUpdate(
        accountEmail,
        [activationTime](api::AccountData& account)
        {
            account.statusCode = api::AccountStatus::activated;
            account.activationTime = activationTime;
        });
}

nx::sql::DBResult AccountManager::verifyAccount(
    nx::sql::QueryContext* const queryContext,
    const data::AccountConfirmationCode& verificationCode,
    std::string* const resultAccountEmail )
{
    std::string accountEmail;
    auto dbResult = m_dao->getAccountEmailByVerificationCode(
        queryContext, verificationCode, &accountEmail);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = m_dao->removeVerificationCode(
        queryContext, verificationCode);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    const auto accountActivationTime = nx::utils::utcTime();
    dbResult = m_dao->updateAccountToActiveStatus(
        queryContext, accountEmail, accountActivationTime);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    auto account = m_dao->fetchAccountByEmail(queryContext, accountEmail);
    if (!account)
        return nx::sql::DBResult::ok;

    // TODO: #ak Have to call some method like afterChangingAccountStatus -
    // will do it in default branch to minimize this change.
    m_extensions.invoke(
        &AbstractAccountManagerExtension::afterUpdatingAccountPassword,
        queryContext,
        *account);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&AccountManager::activateAccountInCache, this,
            accountEmail, accountActivationTime));

    *resultAccountEmail = std::move(accountEmail);

    return nx::sql::DBResult::ok;
}

void AccountManager::sendActivateAccountResponse(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::sql::DBResult resultCode,
    data::AccountConfirmationCode /*verificationCode*/,
    std::string accountEmail,
    std::function<void(api::Result, api::AccountEmail)> completionHandler)
{
    if (resultCode != nx::sql::DBResult::ok)
    {
        return completionHandler(
            dbResultToApiResult(resultCode),
            api::AccountEmail());
    }

    api::AccountEmail response;
    response.email = accountEmail;
    completionHandler(
        dbResultToApiResult(resultCode),
        std::move(response));
}

nx::sql::DBResult AccountManager::updateAccountInDb(
    bool activateAccountIfNotActive,
    nx::sql::QueryContext* const queryContext,
    const data::AccountUpdateDataWithEmail& accountUpdateData)
{
    if (!isValidInput(accountUpdateData))
        return nx::sql::DBResult::cancelled;

    m_dao->updateAccount(
        queryContext,
        accountUpdateData.email,
        accountUpdateData);

    if (activateAccountIfNotActive)
    {
        auto account = m_dao->fetchAccountByEmail(queryContext, accountUpdateData.email);
        if (!account)
            return nx::sql::DBResult::notFound;

        if (account->statusCode != api::AccountStatus::activated)
        {
            const auto activationTime = nx::utils::utcTime();
            const auto dbResult = m_dao->updateAccountToActiveStatus(
                queryContext,
                accountUpdateData.email,
                activationTime);
            if (dbResult != nx::sql::DBResult::ok)
                return dbResult;

            queryContext->transaction()->addOnSuccessfulCommitHandler(
                std::bind(&AccountManager::activateAccountInCache, this,
                    accountUpdateData.email, activationTime));
        }
    }

    if (accountUpdateData.passwordHa1 || accountUpdateData.passwordHa1Sha256)
    {
        m_tempPasswordManager->removeTemporaryPasswordsFromDbByAccountEmail(
            queryContext,
            accountUpdateData.email);

        auto account = m_dao->fetchAccountByEmail(queryContext, accountUpdateData.email);
        if (!account)
            return nx::sql::DBResult::notFound;

        m_extensions.invoke(
            &AbstractAccountManagerExtension::afterUpdatingAccountPassword,
            queryContext,
            *account);
    }

    m_extensions.invoke(
        &AbstractAccountManagerExtension::afterUpdatingAccount,
        queryContext,
        accountUpdateData);

    return nx::sql::DBResult::ok;
}

bool AccountManager::isValidInput(const data::AccountUpdateDataWithEmail& accountData) const
{
    NX_ASSERT(
        static_cast<bool>(accountData.passwordHa1) ||
        static_cast<bool>(accountData.passwordHa1Sha256) ||
        static_cast<bool>(accountData.fullName) ||
        static_cast<bool>(accountData.customization));

    if (!(accountData.passwordHa1 || accountData.passwordHa1Sha256 ||
          accountData.fullName || accountData.customization))
    {
        // Nothing to do.
        return false;
    }

    return true;
}

void AccountManager::updateAccountCache(
    data::AccountUpdateDataWithEmail accountData)
{
    m_cache.atomicUpdate(
        accountData.email,
        [&accountData](api::AccountData& account)
        {
            if (accountData.passwordHa1)
                account.passwordHa1 = accountData.passwordHa1.get();
            if (accountData.passwordHa1Sha256)
                account.passwordHa1Sha256 = accountData.passwordHa1Sha256.get();
            if (accountData.fullName)
                account.fullName = accountData.fullName.get();
            if (accountData.customization)
                account.customization = accountData.customization.get();
        });

    if (accountData.passwordHa1 || accountData.passwordHa1Sha256)
    {
        // Removing account's temporary passwords.
        m_tempPasswordManager->
            removeTemporaryPasswordsFromCacheByAccountEmail(
                accountData.email);
    }
}

nx::sql::DBResult AccountManager::issueRestorePasswordCode(
    nx::sql::QueryContext* const queryContext,
    const std::string& accountEmail,
    data::AccountConfirmationCode* const confirmationCode)
{
    data::AccountData account;
    nx::sql::DBResult dbResult = fetchAccountByEmail(queryContext, accountEmail, &account);
    if (dbResult != nx::sql::DBResult::ok)
        return dbResult;

    dbResult = createPasswordResetCode(
        queryContext,
        accountEmail,
        m_settings.accountManager().passwordResetCodeExpirationTimeout,
        confirmationCode);
    if (dbResult != nx::sql::DBResult::ok)
    {
        NX_DEBUG(this, lm("Failed to issue password reset code for account %1")
            .arg(accountEmail));
        return dbResult;
    }

    RestorePasswordNotification notification;
    notification.customization = account.customization;
    notification.setAddressee(accountEmail);
    notification.setActivationCode(confirmationCode->code);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, notification = std::move(notification)]()
        {
            m_emailManager->sendAsync(
                std::move(notification),
                std::function<void(bool)>());
        });

    return nx::sql::DBResult::ok;
}

void AccountManager::temporaryCredentialsSaved(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    api::Result result,
    const std::string& accountEmail,
    api::TemporaryCredentials temporaryCredentials,
    std::function<void(api::Result, api::TemporaryCredentials)> completionHandler)
{
    if (result != api::ResultCode::ok)
    {
        NX_DEBUG(this, lm("%1 (%2). Failed to save temporary credentials (%3)")
            .args(kAccountCreateTemporaryCredentialsPath, accountEmail,
                api::toString(result.code)));
        return completionHandler(
            result,
            api::TemporaryCredentials());
    }

    return completionHandler(
        api::ResultCode::ok,
        std::move(temporaryCredentials));
}

} // namespace nx::cloud::db
