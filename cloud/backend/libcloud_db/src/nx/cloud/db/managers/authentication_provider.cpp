#include "authentication_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <nx/cloud/db/api/cloud_nonce.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/clusterdb/engine/transaction_log.h>

#include "temporary_account_password_manager.h"
#include "../dao/user_authentication_data_object_factory.h"
#include "../settings.h"
#include "../stree/cdb_ns.h"
#include "../access_control/authentication_manager.h"
#include "../ec2/vms_p2p_command_bus.h"

namespace nx::cloud::db {

AuthenticationProvider::AuthenticationProvider(
    const conf::Settings& settings,
    nx::sql::AsyncSqlQueryExecutor* sqlQueryExecutor,
    AbstractAccountManager* accountManager,
    AbstractSystemSharingManager* systemSharingManager,
    const AbstractTemporaryAccountPasswordManager& temporaryAccountCredentialsManager,
    ec2::AbstractVmsP2pCommandBus* vmsP2pCommandBus)
:
    m_settings(settings),
    m_sqlQueryExecutor(sqlQueryExecutor),
    m_accountManager(accountManager),
    m_systemSharingManager(systemSharingManager),
    m_temporaryAccountCredentialsManager(temporaryAccountCredentialsManager),
    m_authenticationDataObject(dao::UserAuthenticationDataObjectFactory::instance().create()),
    m_vmsP2pCommandBus(vmsP2pCommandBus)
{
    m_accountManager->addExtension(this);
    m_systemSharingManager->addSystemSharingExtension(this);

    m_updateExpiredAuthTimer.post(
        std::bind(&AuthenticationProvider::checkForExpiredAuthRecordsAsync, this));
}

AuthenticationProvider::~AuthenticationProvider()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    m_ongoingOperationCounter.wait();
    m_updateExpiredAuthTimer.pleaseStopSync();

    m_systemSharingManager->removeSystemSharingExtension(this);
    m_accountManager->removeExtension(this);
}

void AuthenticationProvider::getCdbNonce(
    const AuthorizationInfo& authzInfo,
    const data::DataFilter& filter,
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    std::string systemId;
    std::string accountEmail;
    if (!authzInfo.get(attr::authSystemId, &systemId))
    {
        if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
            return completionHandler(api::ResultCode::forbidden, api::NonceData());
        if (!filter.get(attr::systemId, &systemId))
            return completionHandler(api::ResultCode::badRequest, api::NonceData());
        const auto accessRole = m_systemSharingManager->getAccountRightsForSystem(
            accountEmail, systemId);
        if (accessRole == api::SystemAccessRole::none)
            return completionHandler(api::ResultCode::forbidden, api::NonceData());
    }

    api::NonceData result;
    result.nonce = api::generateCloudNonceBase(systemId);
    if (!accountEmail.empty())  //< This is a request from portal. Generating real nonce.
        result.nonce = api::generateNonce(result.nonce);
    result.validPeriod = m_settings.auth().nonceValidityPeriod;

    completionHandler(api::ResultCode::ok, std::move(result));
}

void AuthenticationProvider::getAuthenticationResponse(
    const AuthorizationInfo& authzInfo,
    const data::AuthRequest& authRequest,
    std::function<void(api::ResultCode, api::AuthResponse)> completionHandler)
{
    using namespace std::placeholders;

    std::string systemId;
    if (!authzInfo.get(attr::authSystemId, &systemId))
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());
    if (authRequest.realm != AuthenticationManager::realm())
        return completionHandler(api::ResultCode::unknownRealm, api::AuthResponse());

    auto isNonceValid = std::make_shared<bool>(false);
    m_sqlQueryExecutor->executeSelect(
        std::bind(&AuthenticationProvider::validateNonce, this, _1,
            authRequest.nonce, systemId, isNonceValid),
        [this, completionHandler = std::move(completionHandler),
            systemId, authRequest, isNonceValid](
                nx::sql::DBResult dbResult)
        {
            if (dbResult != nx::sql::DBResult::ok)
                return completionHandler(api::ResultCode::dbError, api::AuthResponse());
            if (!(*isNonceValid))
                return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

            auto response = prepareAuthenticationResponse(systemId, authRequest);
            completionHandler(std::get<0>(response), std::get<1>(response));
        });
}

nx::sql::DBResult AuthenticationProvider::afterSharingSystem(
    nx::sql::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    SharingType sharingType)
{
    if (sharingType == SharingType::invite)
        return nx::sql::DBResult::ok;

    const auto nonce = fetchOrCreateNonce(queryContext, sharing.systemId);

    const auto account = m_accountManager->findAccountByUserName(sharing.accountEmail);
    if (!account)
        throw nx::sql::Exception(nx::sql::DBResult::notFound);
    if (account->statusCode != api::AccountStatus::activated)
    {
        NX_VERBOSE(this, lm("Ignoring not-activated account %1").arg(sharing.accountEmail));
        return nx::sql::DBResult::ok;
    }

    addUserAuthRecord(
        queryContext,
        sharing.systemId,
        sharing.vmsUserId,
        *account,
        nonce);
    return nx::sql::DBResult::ok;
}

void AuthenticationProvider::afterUpdatingAccountPassword(
    nx::sql::QueryContext* const queryContext,
    const api::AccountData& account)
{
    const auto systems = m_authenticationDataObject->fetchAccountSystems(
        queryContext, account.id);
    m_authenticationDataObject->deleteAccountAuthRecords(
        queryContext, account.id);
    for (const auto& system: systems)
    {
        addUserAuthRecord(
            queryContext,
            system.systemId,
            system.vmsUserId,
            account,
            system.nonce);
    }
}

boost::optional<AuthenticationProvider::AccountWithEffectivePassword>
    AuthenticationProvider::getAccountByLogin(const std::string& login) const
{
    std::string passwordHa1;
    auto account = m_accountManager->findAccountByUserName(login.c_str());
    if (account)
    {
        passwordHa1 = account->passwordHa1;
    }
    else
    {
        // Trying temporary credentials.
        const auto temporaryCredentials =
            m_temporaryAccountCredentialsManager.getCredentialsByLogin(login);
        if (temporaryCredentials)
        {
            account = m_accountManager->findAccountByUserName(
                temporaryCredentials->accountEmail);
            passwordHa1 = temporaryCredentials->passwordHa1;
        }
    }

    if (!account)
        return boost::none;
    return AccountWithEffectivePassword{
        std::move(*account),
        std::move(passwordHa1)};
}

nx::sql::DBResult AuthenticationProvider::validateNonce(
    nx::sql::QueryContext* queryContext,
    const std::string& nonce,
    const std::string& systemId,
    std::shared_ptr<bool> isNonceValid)
{
    uint32_t timestamp = 0;
    std::string nonceHash;
    if (!api::parseCloudNonceBase(nonce, &timestamp, &nonceHash))
    {
        *isNonceValid = false;
        return nx::sql::DBResult::ok;
    }

    const auto calculatedNonceHash = api::calcNonceHash(systemId, timestamp);
    if (nonceHash != calculatedNonceHash)
    {
        *isNonceValid = false;
        return nx::sql::DBResult::ok;
    }

    auto systemNonce = m_authenticationDataObject->fetchSystemNonce(queryContext, systemId);
    if (systemNonce && *systemNonce == nonce)
    {
        // Nonce of the system is valid regardless of expiration time.
    }
    else if (std::chrono::seconds(timestamp) + m_settings.auth().nonceValidityPeriod <
        nx::utils::timeSinceEpoch())
    {
        *isNonceValid = false;
        return nx::sql::DBResult::ok;
    }

    *isNonceValid = true;
    return nx::sql::DBResult::ok;
}

std::tuple<api::ResultCode, api::AuthResponse>
AuthenticationProvider::prepareAuthenticationResponse(
    const std::string& systemId,
    const data::AuthRequest& authRequest)
{
    auto accountWithEffectivePassword = getAccountByLogin(authRequest.username);
    if (!accountWithEffectivePassword)
        return std::make_tuple(api::ResultCode::badUsername, api::AuthResponse());

    // TODO: #ak: We should have used authorization rule tree here
    if (accountWithEffectivePassword->account.statusCode != api::AccountStatus::activated)
        return std::make_tuple(api::ResultCode::forbidden, api::AuthResponse());
    auto systemSharingData =
        m_systemSharingManager->getSystemSharingData(
            accountWithEffectivePassword->account.email,
            systemId);
    if (!systemSharingData)
        return std::make_tuple(api::ResultCode::forbidden, api::AuthResponse());

    api::AuthResponse response =
        prepareResponse(
            std::move(authRequest.nonce),
            std::move(*systemSharingData),
            std::move(accountWithEffectivePassword->passwordHa1));

    return std::make_tuple(api::ResultCode::ok, std::move(response));
}

api::AuthResponse AuthenticationProvider::prepareResponse(
    std::string nonce,
    api::SystemSharingEx systemSharing,
    const std::string& passwordHa1) const
{
    api::AuthResponse response;
    response.nonce = std::move(nonce);
    response.authenticatedAccountData = std::move(systemSharing);
    response.accessRole = response.authenticatedAccountData.accessRole;
    const auto intermediateResponse = nx::network::http::calcIntermediateResponse(
        passwordHa1.c_str(),
        response.nonce.c_str());
    response.intermediateResponse.assign(
        intermediateResponse.constData(),
        intermediateResponse.size());
    response.validPeriod = m_settings.auth().intermediateResponseValidityPeriod;

    return response;
}

std::string AuthenticationProvider::fetchOrCreateNonce(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    auto nonce = m_authenticationDataObject->fetchSystemNonce(
        queryContext, systemId);
    if (!nonce)
    {
        nonce = api::generateCloudNonceBase(systemId);
        m_authenticationDataObject->insertOrReplaceSystemNonce(
            queryContext, systemId, *nonce);
    }
    return *nonce;
}

void AuthenticationProvider::addUserAuthRecord(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& vmsUserId,
    const api::AccountData& account,
    const std::string& nonce)
{
    NX_DEBUG(this,
        lm("Updating authentication information of user %1 (vms user id %2) of system %3")
        .arg(account.email).arg(vmsUserId).arg(systemId));

    auto authRecord = generateAuthRecord(account, nonce);

    api::AuthInfo userAuthRecords;
    userAuthRecords.records.push_back(std::move(authRecord));
    m_authenticationDataObject->insertUserAuthRecords(
        queryContext, systemId, account.id, userAuthRecords);

    generateUpdateUserAuthInfoTransaction(
        queryContext, systemId, vmsUserId, userAuthRecords);
}

api::AuthInfoRecord AuthenticationProvider::generateAuthRecord(
    const api::AccountData& account,
    const std::string& nonce)
{
    api::AuthInfoRecord authInfo;
    authInfo.nonce = nonce;
    authInfo.intermediateResponse = nx::network::http::calcIntermediateResponse(
        account.passwordHa1.c_str(), nonce.c_str()).toStdString();
    authInfo.expirationTime =
        nx::utils::utcTime() + m_settings.auth().offlineUserHashValidityPeriod;
    return authInfo;
}

void AuthenticationProvider::generateUpdateUserAuthInfoTransaction(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& vmsUserId,
    const api::AuthInfo& userAuthenticationRecords)
{
    nx::vms::api::ResourceParamWithRefData userAuthenticationInfoAttribute;
    userAuthenticationInfoAttribute.name = api::kVmsUserAuthInfoAttributeName;
    userAuthenticationInfoAttribute.resourceId =
        QnUuid::fromStringSafe(vmsUserId.c_str());
    userAuthenticationInfoAttribute.value =
        QString::fromUtf8(QJson::serialized(userAuthenticationRecords));
    const auto dbResult = m_vmsP2pCommandBus->saveResourceAttribute(
        queryContext,
        systemId,
        std::move(userAuthenticationInfoAttribute));
    if (dbResult != nx::sql::DBResult::ok)
        throw nx::sql::Exception(dbResult);
}

void AuthenticationProvider::checkForExpiredAuthRecordsAsync()
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    m_sqlQueryExecutor->executeUpdate(
        std::bind(&AuthenticationProvider::checkForExpiredAuthRecords, this, _1),
        [this, currentRequestIncrement = m_ongoingOperationCounter.getScopedIncrement()](
            sql::DBResult result)
        {
            startCheckForExpiredAuthRecordsTimer(result);
        });
}

sql::DBResult AuthenticationProvider::checkForExpiredAuthRecords(
    sql::QueryContext* queryContext)
{
    const auto systemsWithExpiredAuthRecords =
        m_authenticationDataObject->fetchSystemsWithExpiredAuthRecords(
            queryContext, m_settings.auth().maxSystemsToUpdateAtATime);
    if (systemsWithExpiredAuthRecords.empty())
    {
        NX_VERBOSE(this, lm("No systems with expired user authentication records"));
        return sql::DBResult::notFound;
    }

    NX_DEBUG(this, lm("Found %1 systems with expired user authentication records")
        .args(systemsWithExpiredAuthRecords.size()));

    for (const auto& systemId: systemsWithExpiredAuthRecords)
        updateSystemAuth(queryContext, systemId);

    return sql::DBResult::ok;
}

void AuthenticationProvider::updateSystemAuth(
    sql::QueryContext* queryContext,
    const std::string& systemId)
{
    NX_VERBOSE(this, lm("Updating system %1 user authentication records").args(systemId));

    try
    {
        const auto nonce = api::generateCloudNonceBase(systemId);
        m_authenticationDataObject->insertOrReplaceSystemNonce(
            queryContext, systemId, nonce);

        m_authenticationDataObject->deleteSystemAuthRecords(
            queryContext, systemId);

        std::vector<api::SystemSharingEx> users =
            m_systemSharingManager->fetchSystemUsers(queryContext, systemId);
        for (const auto& user: users)
            updateUserAuthInSystem(queryContext, systemId, nonce, user);

        NX_DEBUG(this, lm("Updated system %1 user authentication records").args(systemId));
    }
    catch (const std::exception& e)
    {
        NX_DEBUG(this, lm("Error updating system %1 user authentication records. %2")
            .args(systemId, e.what()));
        throw;
    }
}

void AuthenticationProvider::updateUserAuthInSystem(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const std::string& nonce,
    const api::SystemSharingEx& userSharing)
{
    NX_VERBOSE(this, lm("Updating user %1 authentication records for system %2 using nonce %3")
        .args(userSharing.accountEmail, systemId, nonce));

    data::AccountData account;
    const auto dbResult = m_accountManager->fetchAccountByEmail(
        queryContext, userSharing.accountEmail, &account);
    if (dbResult != sql::DBResult::ok)
        throw sql::Exception(dbResult);
    if (account.passwordHa1.empty() && account.passwordHa1Sha256.empty())
    {
        NX_VERBOSE(this, lm("Skipping user %1, system %2. User does not have password")
            .args(userSharing.accountEmail, systemId));
        return;
    }

    addUserAuthRecord(
        queryContext,
        systemId,
        userSharing.vmsUserId,
        account,
        nonce);

    NX_DEBUG(this, lm("Updated user %1 authentication records for system %2 using nonce %3")
        .args(userSharing.accountEmail, systemId, nonce));
}

void AuthenticationProvider::startCheckForExpiredAuthRecordsTimer(sql::DBResult result)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    m_updateExpiredAuthTimer.start(
        result == sql::DBResult::ok //< Updated some records?
            ? m_settings.auth().continueUpdatingExpiredAuthPeriod
            : m_settings.auth().checkForExpiredAuthPeriod,
        std::bind(&AuthenticationProvider::checkForExpiredAuthRecordsAsync, this));
}

} // namespace nx::cloud::db
