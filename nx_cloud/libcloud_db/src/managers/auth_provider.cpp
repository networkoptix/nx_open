/**********************************************************
* Sep 28, 2015
* a.kolesnikov
***********************************************************/

#include "auth_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <cdb/cloud_nonce.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>
#include <nx/utils/uuid.h>
#include <utils/common/util.h>

#include "access_control/authentication_manager.h"
#include "account_manager.h"
#include "settings.h"
#include "stree/cdb_ns.h"
#include "system_manager.h"


namespace nx {
namespace cdb {

AuthenticationProvider::AuthenticationProvider(
    const conf::Settings& settings,
    const AccountManager& accountManager,
    const SystemManager& systemManager)
:
    m_settings(settings),
    m_accountManager(accountManager),
    m_systemManager(systemManager)
{
}

void AuthenticationProvider::getCdbNonce(
    const AuthorizationInfo& authzInfo,
    const data::DataFilter& filter,
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    std::string systemID;
    std::string accountEmail;
    if (!authzInfo.get(attr::authSystemID, &systemID))
    {
        if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
            return completionHandler(api::ResultCode::forbidden, api::NonceData());
        if (!filter.get(attr::systemID, &systemID))
            return completionHandler(api::ResultCode::badRequest, api::NonceData());
        const auto accessRole = m_systemManager.getAccountRightsForSystem(
            accountEmail, systemID);
        if (accessRole == api::SystemAccessRole::none)
            return completionHandler(api::ResultCode::forbidden, api::NonceData());
    }

    api::NonceData result;
    result.nonce = api::generateCloudNonceBase(systemID);
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
    //{random_3_bytes}base64({ timestamp }MD5(systemID:timestamp:secret_key))

    std::string systemID;
    if (!authzInfo.get(attr::authSystemID, &systemID))
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());
    if (authRequest.realm != AuthenticationManager::realm())
        return completionHandler(api::ResultCode::unknownRealm, api::AuthResponse());

    //checking that nonce is valid and corresponds to the systemID
    uint32_t timestamp = 0;
    std::string nonceHash;
    if (!api::parseCloudNonceBase(authRequest.nonce, &timestamp, &nonceHash))
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

    if (std::chrono::seconds(timestamp) + m_settings.auth().nonceValidityPeriod <
        nx::utils::timeSinceEpoch())
    {
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());
    }

    const auto calculatedNonceHash = api::calcNonceHash(systemID, timestamp);
    if (nonceHash != calculatedNonceHash)
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

    //checking that username corresponds to account user and requested system is shared with that user
    //TODO #ak we should use stree here
    const auto account = 
        m_accountManager.findAccountByUserName(authRequest.username.c_str());
    if (!account)
        return completionHandler(api::ResultCode::badUsername, api::AuthResponse());
    if (account->statusCode != api::AccountStatus::activated)
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());

    auto systemSharingData = 
        m_systemManager.getSystemSharingData(account->email, systemID);
    if (!static_cast<bool>(systemSharingData))
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());

    api::AuthResponse response;
    response.nonce = authRequest.nonce;
    response.authenticatedAccountData = std::move(*systemSharingData);
    response.accessRole = response.authenticatedAccountData.accessRole;
    const auto intermediateResponse = nx_http::calcIntermediateResponse(
        account->passwordHa1.c_str(),
        authRequest.nonce.c_str());
    response.intermediateResponse.assign(
        intermediateResponse.constData(),
        intermediateResponse.size());
    response.validPeriod = m_settings.auth().intermediateResponseValidityPeriod;

    completionHandler(api::ResultCode::ok, std::move(response));
}

}   //cdb
}   //nx
