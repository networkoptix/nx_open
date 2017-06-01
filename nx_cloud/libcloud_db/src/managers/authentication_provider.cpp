#include "authentication_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <cdb/cloud_nonce.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>
#include <nx/utils/uuid.h>

#include "access_control/authentication_manager.h"
#include "account_manager.h"
#include "temporary_account_password_manager.h"
#include "settings.h"
#include "stree/cdb_ns.h"
#include "system_manager.h"

namespace nx {
namespace cdb {

AuthenticationProvider::AuthenticationProvider(
    const conf::Settings& settings,
    const AccountManager& accountManager,
    const SystemManager& systemManager,
    const TemporaryAccountPasswordManager& temporaryAccountCredentialsManager)
:
    m_settings(settings),
    m_accountManager(accountManager),
    m_systemManager(systemManager),
    m_temporaryAccountCredentialsManager(temporaryAccountCredentialsManager)
{
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
        const auto accessRole = m_systemManager.getAccountRightsForSystem(
            accountEmail, systemId);
        if (accessRole == api::SystemAccessRole::none)
            return completionHandler(api::ResultCode::forbidden, api::NonceData());
    }

    api::NonceData result;
    result.nonce = api::generateCloudNonceBase(systemId);
    if (!accountEmail.empty())  //this is a request from portal. Generating real nonce
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
        m_systemManager.getSystemSharingData(
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

boost::optional<AuthenticationProvider::AccountWithEffectivePassword>
    AuthenticationProvider::getAccountByLogin(const std::string& login) const
{
    std::string passwordHa1;
    auto account =
        m_accountManager.findAccountByUserName(login.c_str());
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

} // namespace cdb
} // namespace nx
