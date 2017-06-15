#include "authentication_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/cdb/client/data/auth_data.h>

#include "account_manager.h"
#include "temporary_account_password_manager.h"
#include "../dao/user_authentication_data_object_factory.h"
#include "../settings.h"
#include "../stree/cdb_ns.h"
#include "../access_control/authentication_manager.h"
#include "../ec2/transaction_log.h"
#include "../ec2/vms_p2p_command_bus.h"

namespace nx {
namespace cdb {

AuthenticationProvider::AuthenticationProvider(
    const conf::Settings& settings,
    const AbstractAccountManager& accountManager,
    AbstractSystemSharingManager* systemSharingManager,
    const AbstractTemporaryAccountPasswordManager& temporaryAccountCredentialsManager,
    ec2::AbstractVmsP2pCommandBus* vmsP2pCommandBus)
:
    m_settings(settings),
    m_accountManager(accountManager),
    m_systemSharingManager(systemSharingManager),
    m_temporaryAccountCredentialsManager(temporaryAccountCredentialsManager),
    m_authenticationDataObject(dao::UserAuthenticationDataObjectFactory::instance().create()),
    m_vmsP2pCommandBus(vmsP2pCommandBus)
{
    m_systemSharingManager->addSystemSharingExtension(this);
}

AuthenticationProvider::~AuthenticationProvider()
{
    m_systemSharingManager->removeSystemSharingExtension(this);
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
    std::string systemId;
    if (!authzInfo.get(attr::authSystemId, &systemId))
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());
    if (authRequest.realm != AuthenticationManager::realm())
        return completionHandler(api::ResultCode::unknownRealm, api::AuthResponse());

    if (!validateNonce(authRequest.nonce, systemId))
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

    auto accountWithEffectivePassword = getAccountByLogin(authRequest.username);
    if (!accountWithEffectivePassword)
        return completionHandler(api::ResultCode::badUsername, api::AuthResponse());

    // TODO: #ak: We should have used authorization rule tree here
    if (accountWithEffectivePassword->account.statusCode != api::AccountStatus::activated)
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());
    auto systemSharingData =
        m_systemSharingManager->getSystemSharingData(
            accountWithEffectivePassword->account.email,
            systemId);
    if (!systemSharingData)
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());

    api::AuthResponse response = 
        prepareResponse(
            std::move(authRequest.nonce),
            std::move(*systemSharingData),
            std::move(accountWithEffectivePassword->passwordHa1));

    completionHandler(api::ResultCode::ok, std::move(response));
}

nx::db::DBResult AuthenticationProvider::afterSharingSystem(
    nx::db::QueryContext* const queryContext,
    const api::SystemSharing& sharing,
    SharingType sharingType)
{
    if (sharingType == SharingType::invite)
        return nx::db::DBResult::ok;

    NX_DEBUG(this, lm("Updating authentication information of user %1 of system %2")
        .arg(sharing.accountEmail).arg(sharing.systemId));

    const auto nonce = fetchOrCreateNonce(queryContext, sharing.systemId);
        
    const auto account = m_accountManager.findAccountByUserName(sharing.accountEmail);
    if (!account)
        throw nx::db::Exception(nx::db::DBResult::notFound);
    if (account->statusCode != api::AccountStatus::activated)
    {
        NX_VERBOSE(this, lm("Ignoring not-activated account %1").arg(sharing.accountEmail));
        return nx::db::DBResult::ok;
    }

    addUserAuthRecord(
        queryContext,
        sharing.systemId,
        sharing.vmsUserId,
        *account,
        nonce);
    return nx::db::DBResult::ok;
}

void AuthenticationProvider::afterUpdatingAccountPassword(
    nx::db::QueryContext* const queryContext,
    const api::AccountData& account)
{
    m_authenticationDataObject->deleteAccountAuthRecords(
        queryContext, account.id);

    const auto systems = m_authenticationDataObject->fetchAccountSystems(
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
    auto account = m_accountManager.findAccountByUserName(login.c_str());
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
            account = m_accountManager.findAccountByUserName(
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

bool AuthenticationProvider::validateNonce(
    const std::string& nonce,
    const std::string& systemId) const
{
    uint32_t timestamp = 0;
    std::string nonceHash;
    if (!api::parseCloudNonceBase(nonce, &timestamp, &nonceHash))
        return false;

    if (std::chrono::seconds(timestamp) + m_settings.auth().nonceValidityPeriod <
        nx::utils::timeSinceEpoch())
    {
        return false;
    }

    const auto calculatedNonceHash = api::calcNonceHash(systemId, timestamp);
    if (nonceHash != calculatedNonceHash)
        return false;

    return true;
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
    const auto intermediateResponse = nx_http::calcIntermediateResponse(
        passwordHa1.c_str(),
        response.nonce.c_str());
    response.intermediateResponse.assign(
        intermediateResponse.constData(),
        intermediateResponse.size());
    response.validPeriod = m_settings.auth().intermediateResponseValidityPeriod;

    return response;
}

std::string AuthenticationProvider::fetchOrCreateNonce(
    nx::db::QueryContext* const queryContext,
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
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& vmsUserId,
    const api::AccountData& account,
    const std::string& nonce)
{
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
    authInfo.intermediateResponse = nx_http::calcIntermediateResponse(
        account.passwordHa1.c_str(), nonce.c_str()).toStdString();
    authInfo.expirationTime = 
        nx::utils::utcTime() + m_settings.auth().offlineUserHashValidityPeriod;
    return authInfo;
}

void AuthenticationProvider::removeExpiredRecords(
    api::AuthInfo* /*userAuthenticationRecords*/)
{
    // TODO
}

void AuthenticationProvider::generateUpdateUserAuthInfoTransaction(
    nx::db::QueryContext* const queryContext,
    const std::string& systemId,
    const std::string& vmsUserId,
    const api::AuthInfo& userAuthenticationRecords)
{
    ::ec2::ApiResourceParamWithRefData userAuthenticationInfoAttribute;
    userAuthenticationInfoAttribute.name = api::kVmsUserAuthInfoAttributeName;
    userAuthenticationInfoAttribute.resourceId = 
        QnUuid::fromStringSafe(vmsUserId.c_str());
    userAuthenticationInfoAttribute.value = 
        QString::fromUtf8(QJson::serialized(userAuthenticationRecords));
    const auto dbResult = m_vmsP2pCommandBus->saveResourceAttribute(
        queryContext,
        systemId,
        std::move(userAuthenticationInfoAttribute));
    if (dbResult != nx::db::DBResult::ok)
        throw nx::db::Exception(dbResult);
}

} // namespace cdb
} // namespace nx
