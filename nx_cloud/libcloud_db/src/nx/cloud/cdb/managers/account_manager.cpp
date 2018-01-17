#include "account_manager.h"

#include <chrono>
#include <functional>
#include <iostream>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <nx/cloud/cdb/client/cdb_request_path.h>
#include <nx/cloud/cdb/client/data/types.h>
#include <nx/email/mustache/mustache_helper.h>
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

namespace nx {
namespace cdb {

AccountManager::AccountManager(
    const conf::Settings& settings,
    const StreeManager& streeManager,
    AbstractTemporaryAccountPasswordManager* const tempPasswordManager,
    nx::utils::db::AsyncSqlQueryExecutor* const dbManager,
    AbstractEmailManager* const emailManager) noexcept(false)
:
    m_settings(settings),
    m_streeManager(streeManager),
    m_tempPasswordManager(tempPasswordManager),
    m_dbManager(dbManager),
    m_emailManager(emailManager),
    m_dao(dao::AccountDataObjectFactory::instance().create())
{
    if( fillCache() != nx::utils::db::DBResult::ok )
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
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const nx::utils::stree::AbstractResourceReader& authSearchInputData,
    nx::utils::stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    {
        QnMutexLocker lk(&m_mutex);

        auto account = m_cache.find(toStdString(username));
        if (account && validateHa1Func(account->passwordHa1.c_str()))
        {
            authProperties->put(
                cdb::attr::authAccountEmail,
                username);
            completionHandler(api::ResultCode::ok);
            return;
        }
    }

    //NOTE currently, all authenticateByName are sync.
    //  Changing it to async requires some HTTP authentication refactoring

    //authenticating by temporary password
    m_tempPasswordManager->authenticateByName(
        username,
        std::move(validateHa1Func),
        authSearchInputData,
        authProperties,
        [username, authProperties, completionHandler = std::move(completionHandler), this](
            api::ResultCode authResult) mutable
        {
            if (authResult == api::ResultCode::ok)
            {
                bool authenticatedByEmailCode = false;
                authProperties->get(
                    attr::authenticatedByEmailCode,
                    &authenticatedByEmailCode);
                if (authenticatedByEmailCode)
                {
                    //activating account
                    m_cache.atomicUpdate(
                        std::string(username.constData(), username.size()),
                        [](api::AccountData& account) {
                            account.statusCode = api::AccountStatus::activated;
                        });
                }
            }
            completionHandler(authResult);
        });
}

void AccountManager::registerAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountRegistrationData accountRegistrationData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
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
                nx::utils::db::QueryContext* const /*queryContext*/,
                nx::utils::db::DBResult dbResult,
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
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountConfirmationCode, std::string>(
        std::bind(&AccountManager::verifyAccount, this, _1, _2, _3),
        std::move(emailVerificationCode),
        std::bind(&AccountManager::sendActivateAccountResponse, this,
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, _4, std::move(completionHandler)));
}

void AccountManager::getAccount(
    const AuthorizationInfo& authzInfo,
    std::function<void(api::ResultCode, data::AccountData)> completionHandler )
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
    std::function<void(api::ResultCode)> completionHandler)
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
                nx::utils::db::QueryContext* /*queryContext*/,
                nx::utils::db::DBResult resultCode,
                data::AccountUpdateDataWithEmail accountData)
        {
            if (resultCode == nx::utils::db::DBResult::ok)
                updateAccountCache(authenticatedByEmailCode, std::move(accountData));
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
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    auto account = m_cache.find(accountEmail.email);
    if (!account)
    {
        NX_LOG(lm("%1 (%2). Account not found").
            arg(kAccountPasswordResetPath).arg(accountEmail.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    //fetching request source
    bool hasRequestCameFromSecureSource = false;
    authzInfo.get(attr::secureSource, &hasRequestCameFromSecureSource);

    m_dbManager->executeUpdate<std::string, data::AccountConfirmationCode>(
        [this](
            nx::utils::db::QueryContext* const queryContext,
            const std::string& accountEmail,
            data::AccountConfirmationCode* const confirmationCode)
        {
            return resetPassword(queryContext, accountEmail, confirmationCode);
        },
        accountEmail.email,
        [this, hasRequestCameFromSecureSource,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::utils::db::QueryContext* /*queryContext*/,
                nx::utils::db::DBResult dbResultCode,
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
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    const auto existingAccount = m_cache.find(accountEmail.email);
    if (!existingAccount)
    {
        NX_LOG(lm("/account/reactivate. Unknown account %1").
            arg(accountEmail.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    if (existingAccount->statusCode == api::AccountStatus::activated)
    {
        NX_LOG(lm("/account/reactivate. Not reactivating of already activated account %1").
            arg(accountEmail.email), cl_logDEBUG1);
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
        [this, notification = std::move(notification)](
            nx::utils::db::QueryContext* const queryContext,
            const std::string& accountEmail,
            data::AccountConfirmationCode* const resultData) mutable
        {
            return issueAccountActivationCode(
                queryContext,
                accountEmail,
                std::move(notification),
                resultData);
        },
        std::move(accountEmail.email),
        std::bind(&AccountManager::accountReactivated, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            requestSourceSecured,
            _1, _2, _3, _4, std::move(completionHandler)));
}

void AccountManager::createTemporaryCredentials(
    const AuthorizationInfo& authzInfo,
    data::TemporaryCredentialsParams params,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
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
        NX_LOG(lm("%1 (%2). Account not found").
            arg(kAccountCreateTemporaryCredentialsPath).arg(accountEmail), cl_logDEBUG1);
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
    //temporary login is generated by \a addRandomCredentials
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
    tempPasswordData.accessRights.requestsDenied.push_back(kAccountUpdatePath);
    tempPasswordData.accessRights.requestsDenied.push_back(kAccountPasswordResetPath);
    m_tempPasswordManager->addRandomCredentials(&tempPasswordData);

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

nx::utils::db::DBResult AccountManager::insertAccount(
    nx::utils::db::QueryContext* const queryContext,
    data::AccountData account)
{
    if (account.id.empty())
        account.id = generateNewAccountId();
    if (account.customization.empty())
        account.customization = nx::utils::AppInfo::customizationName().toStdString();

    account.registrationTime = nx::utils::utcTime();

    const auto dbResult = m_dao->insert(queryContext, account);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, account = std::move(account)]()
        {
            auto email = account.email;
            m_cache.insert(std::move(email), std::move(account));
        });

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult AccountManager::updateAccount(
    nx::utils::db::QueryContext* const queryContext,
    data::AccountData account)
{
    NX_ASSERT(!account.id.empty() && !account.email.empty());
    if (account.id.empty() || account.email.empty())
        return nx::utils::db::DBResult::ioError;

    auto dbResult = m_dao->update(queryContext, account);
    if (dbResult != nx::utils::db::DBResult::ok)
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

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult AccountManager::fetchAccountByEmail(
    nx::utils::db::QueryContext* queryContext,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    return m_dao->fetchAccountByEmail(
        queryContext,
        accountEmail,
        accountData);
}

nx::utils::db::DBResult AccountManager::createPasswordResetCode(
    nx::utils::db::QueryContext* const queryContext,
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
    tempPasswordData.accessRights.requestsAllowed.push_back(kAccountUpdatePath);

    data::Credentials credentials;
    auto dbResultCode = m_tempPasswordManager->fetchTemporaryCredentials(
        queryContext,
        tempPasswordData,
        &credentials);
    if (dbResultCode != nx::utils::db::DBResult::ok &&
        dbResultCode != nx::utils::db::DBResult::notFound)
    {
        return dbResultCode;
    }

    std::string temporaryPassword;
    if (dbResultCode == nx::utils::db::DBResult::ok)
    {
        NX_LOGX(lm("Found existing password reset code (%1) for account %2")
            .arg(credentials.password).arg(accountEmail), cl_logDEBUG2);
        temporaryPassword = credentials.password;
    }
    else
    {
        m_tempPasswordManager->addRandomCredentials(&tempPasswordData);
        temporaryPassword = tempPasswordData.password;

        dbResultCode = m_tempPasswordManager->registerTemporaryCredentials(
            queryContext,
            std::move(tempPasswordData));
        if (dbResultCode != nx::utils::db::DBResult::ok)
            return dbResultCode;
    }

    // Preparing confirmation code.
    auto resetCodeStr = temporaryPassword + ":" + accountEmail;
    confirmationCode->code = QByteArray::fromRawData(
        resetCodeStr.data(), (int)resetCodeStr.size()).toBase64().toStdString();

    return nx::utils::db::DBResult::ok;
}

void AccountManager::addExtension(AbstractAccountManagerExtension* extension)
{
    m_extensions.add(extension);
}

void AccountManager::removeExtension(AbstractAccountManagerExtension* extension)
{
    m_extensions.remove(extension);
}

nx::utils::db::DBResult AccountManager::fillCache()
{
    nx::utils::promise<nx::utils::db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect(
        std::bind(&AccountManager::fetchAccounts, this, _1),
        [&cacheFilledPromise](
            nx::utils::db::QueryContext* /*queryContext*/,
            nx::utils::db::DBResult dbResult)
        {
            cacheFilledPromise.set_value( dbResult );
        });

    //waiting for completion
    future.wait();
    return future.get();
}

nx::utils::db::DBResult AccountManager::fetchAccounts(
    nx::utils::db::QueryContext* queryContext)
{
    std::vector<data::AccountData> accounts;
    auto dbResult = m_dao->fetchAccounts(queryContext, &accounts);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    for (auto& account: accounts)
    {
        auto emailCopy = account.email;
        if (!m_cache.insert(std::move(emailCopy), std::move(account)))
        {
            NX_ASSERT(false);
        }
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult AccountManager::registerNewAccountInDb(
    nx::utils::db::QueryContext* const queryContext,
    const data::AccountData& accountData,
    data::AccountConfirmationCode* const confirmationCode)
{
    data::AccountData existingAccount;
    auto dbResult = fetchAccountByEmail(
        queryContext,
        accountData.email,
        &existingAccount);
    if (dbResult != nx::utils::db::DBResult::ok &&
        dbResult != nx::utils::db::DBResult::notFound)
    {
        NX_LOGX(lm("Failed to fetch account by email %1").arg(accountData.email), cl_logDEBUG1);
        return dbResult;
    }

    if (dbResult == nx::utils::db::DBResult::ok)
    {
        if (existingAccount.statusCode != api::AccountStatus::invited)
        {
            NX_LOG(lm("Failed to add account with already used email %1, status %2").
                arg(accountData.email).arg(QnLexical::serialized(existingAccount.statusCode)),
                cl_logDEBUG1);
            return nx::utils::db::DBResult::uniqueConstraintViolation;
        }

        existingAccount.statusCode = api::AccountStatus::awaitingActivation;
        // Merging existing account with new one.
        auto accountIdBak = std::move(existingAccount.id);
        existingAccount = accountData;
        existingAccount.id = std::move(accountIdBak);

        dbResult = updateAccount(queryContext, std::move(existingAccount));
        if (dbResult != nx::utils::db::DBResult::ok)
        {
            NX_LOGX(lm("Failed to update existing account %1")
                .arg(existingAccount.email), cl_logDEBUG1);
            return dbResult;
        }
    }
    else if (dbResult == nx::utils::db::DBResult::notFound)
    {
        dbResult = insertAccount(queryContext, accountData);
        if (dbResult != nx::utils::db::DBResult::ok)
        {
            NX_LOGX(lm("Failed to insert new account. Email %1")
                .arg(accountData.email), cl_logDEBUG1);
            return dbResult;
        }
    }
    else
    {
        NX_ASSERT(false);
        return nx::utils::db::DBResult::ioError;
    }

    auto notification = std::make_unique<ActivateAccountNotification>();
    notification->customization = accountData.customization;
    return issueAccountActivationCode(
        queryContext,
        accountData.email,
        std::move(notification),
        confirmationCode);
}

nx::utils::db::DBResult AccountManager::issueAccountActivationCode(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::unique_ptr<AbstractActivateAccountNotification> notification,
    data::AccountConfirmationCode* const resultData)
{
    auto verificationCode = m_dao->getVerificationCodeByAccountEmail(
        queryContext,
        accountEmail);
    if (verificationCode)
    {
        resultData->code = *verificationCode;
    }
    else
    {
        auto emailVerificationCode = QnUuid::createUuid().toByteArray().toHex().toStdString();
        const auto codeExpirationTime =
            QDateTime::currentDateTimeUtc().addSecs(
                std::chrono::duration_cast<std::chrono::seconds>(
                    m_settings.accountManager().accountActivationCodeExpirationTimeout).count());

        m_dao->insertEmailVerificationCode(
            queryContext,
            accountEmail,
            emailVerificationCode,
            codeExpirationTime);

        resultData->code = std::move(emailVerificationCode);
    }

    notification->setActivationCode(resultData->code);
    notification->setAddressee(accountEmail);
    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, notification = std::move(notification)]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });

    return nx::utils::db::DBResult::ok;
}

void AccountManager::accountReactivated(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    bool requestSourceSecured,
    nx::utils::db::QueryContext* /*queryContext*/,
    nx::utils::db::DBResult resultCode,
    std::string /*email*/,
    data::AccountConfirmationCode resultData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    //adding activation code only in response to portal
    completionHandler(
        resultCode == nx::utils::db::DBResult::ok
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

nx::utils::db::DBResult AccountManager::verifyAccount(
    nx::utils::db::QueryContext* const queryContext,
    const data::AccountConfirmationCode& verificationCode,
    std::string* const resultAccountEmail )
{
    std::string accountEmail;
    auto dbResult = m_dao->getAccountEmailByVerificationCode(
        queryContext, verificationCode, &accountEmail);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    dbResult = m_dao->removeVerificationCode(
        queryContext, verificationCode);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    const auto accountActivationTime = nx::utils::utcTime();
    dbResult = m_dao->updateAccountToActiveStatus(
        queryContext, accountEmail, accountActivationTime);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&AccountManager::activateAccountInCache, this,
            accountEmail, accountActivationTime));

    *resultAccountEmail = std::move(accountEmail);

    return nx::utils::db::DBResult::ok;
}

void AccountManager::sendActivateAccountResponse(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::utils::db::QueryContext* /*queryContext*/,
    nx::utils::db::DBResult resultCode,
    data::AccountConfirmationCode /*verificationCode*/,
    std::string accountEmail,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler)
{
    if (resultCode != nx::utils::db::DBResult::ok)
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

nx::utils::db::DBResult AccountManager::updateAccountInDb(
    bool activateAccountIfNotActive,
    nx::utils::db::QueryContext* const queryContext,
    const data::AccountUpdateDataWithEmail& accountUpdateData)
{
    if (!isValidInput(accountUpdateData))
        return nx::utils::db::DBResult::cancelled;

    m_dao->updateAccount(
        queryContext,
        accountUpdateData.email,
        accountUpdateData,
        activateAccountIfNotActive);

    if (accountUpdateData.passwordHa1 || accountUpdateData.passwordHa1Sha256)
    {
        auto dbResult = m_tempPasswordManager->removeTemporaryPasswordsFromDbByAccountEmail(
            queryContext,
            accountUpdateData.email);
        if (dbResult != nx::utils::db::DBResult::ok)
            return dbResult;

        data::AccountData account;
        dbResult = m_dao->fetchAccountByEmail(queryContext, accountUpdateData.email, &account);
        if (dbResult != nx::utils::db::DBResult::ok)
            return dbResult;

        m_extensions.invoke(
            &AbstractAccountManagerExtension::afterUpdatingAccountPassword,
            queryContext,
            account);
    }

    m_extensions.invoke(
        &AbstractAccountManagerExtension::afterUpdatingAccount,
        queryContext,
        accountUpdateData);

    return nx::utils::db::DBResult::ok;
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
    bool activateAccountIfNotActive,
    data::AccountUpdateDataWithEmail accountData)
{
    m_cache.atomicUpdate(
        accountData.email,
        [&accountData, activateAccountIfNotActive](api::AccountData& account)
        {
            if (accountData.passwordHa1)
                account.passwordHa1 = accountData.passwordHa1.get();
            if (accountData.passwordHa1Sha256)
                account.passwordHa1Sha256 = accountData.passwordHa1Sha256.get();
            if (accountData.fullName)
                account.fullName = accountData.fullName.get();
            if (accountData.customization)
                account.customization = accountData.customization.get();
            if (activateAccountIfNotActive)
                account.statusCode = api::AccountStatus::activated;
        });

    if (accountData.passwordHa1 || accountData.passwordHa1Sha256)
    {
        // Removing account's temporary passwords.
        m_tempPasswordManager->
            removeTemporaryPasswordsFromCacheByAccountEmail(
                accountData.email);
    }
}

nx::utils::db::DBResult AccountManager::resetPassword(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    data::AccountConfirmationCode* const confirmationCode)
{
    data::AccountData account;
    nx::utils::db::DBResult dbResult = fetchAccountByEmail(queryContext, accountEmail, &account);
    if (dbResult != nx::utils::db::DBResult::ok)
        return dbResult;

    dbResult = createPasswordResetCode(
        queryContext,
        accountEmail,
        m_settings.accountManager().passwordResetCodeExpirationTimeout,
        confirmationCode);
    if (dbResult != nx::utils::db::DBResult::ok)
    {
        NX_LOGX(lm("Failed to issue password reset code for account %1")
            .arg(accountEmail), cl_logDEBUG1);
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

    return nx::utils::db::DBResult::ok;
}

void AccountManager::temporaryCredentialsSaved(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    api::ResultCode resultCode,
    const std::string& accountEmail,
    api::TemporaryCredentials temporaryCredentials,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
{
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOG(lm("%1 (%2). Failed to save temporary credentials (%3)")
            .arg(kAccountCreateTemporaryCredentialsPath).arg(accountEmail)
            .arg((int)resultCode), cl_logDEBUG1);
        return completionHandler(
            resultCode,
            api::TemporaryCredentials());
    }

    return completionHandler(
        api::ResultCode::ok,
        std::move(temporaryCredentials));
}

} // namespace cdb
} // namespace nx
