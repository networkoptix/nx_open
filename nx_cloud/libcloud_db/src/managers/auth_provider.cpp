/**********************************************************
* Sep 28, 2015
* a.kolesnikov
***********************************************************/

#include "auth_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <nx/utils/uuid.h>
#include <utils/common/util.h>
#include <nx/network/http/auth_tools.h>

#include "access_control/authentication_manager.h"
#include "account_manager.h"
#include "system_manager.h"
#include "stree/cdb_ns.h"
#include "settings.h"


namespace nx {
namespace cdb {

namespace {
    const char SECRET_NONCE_KEY[] = "neurod.ru";
    const size_t CDB_NONCE_SIZE = 31;
}

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
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    std::string systemID;
    if (!authzInfo.get(attr::authSystemID, &systemID))
    {
        completionHandler(api::ResultCode::forbidden, api::NonceData());
        return;
    }

    //{random_3_bytes}base64({ timestamp }MD5(systemID:timestamp:secret_key))

    const uint32_t timestamp = std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    const auto md5Hash = calcNonceHash(systemID, timestamp);

    QByteArray random3Bytes;
    random3Bytes.resize(3);
    for(auto& ch: random3Bytes)
        ch = random('a', 'z');
    const auto nonce =
        random3Bytes +
        (QByteArray::fromRawData(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp))
            +md5Hash).toBase64();

    NX_ASSERT(nonce.size() == CDB_NONCE_SIZE);

    api::NonceData result;
    result.nonce.assign(nonce.constData(), nonce.size());
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
    QByteArray random3Bytes;
    uint32_t timestamp = 0;
    QByteArray nonceHash;
    if (!parseNonce(authRequest.nonce, &random3Bytes, &timestamp, &nonceHash))
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

    if (std::chrono::seconds(timestamp) + m_settings.auth().nonceValidityPeriod <
        std::chrono::system_clock::now().time_since_epoch())
    {
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());
    }

    const auto calculatedNonceHash = calcNonceHash(systemID, timestamp);
    if (nonceHash != calculatedNonceHash)
        return completionHandler(api::ResultCode::invalidNonce, api::AuthResponse());

    //checking that username corresponds to account user and requested system is shared with that user
    const auto account = 
        m_accountManager.findAccountByUserName(authRequest.username.c_str());
    if (!account)
        return completionHandler(api::ResultCode::badUsername, api::AuthResponse());
    if (account->statusCode != api::AccountStatus::activated)
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());

    const auto systemAccessRole = 
        m_systemManager.getAccountRightsForSystem(account->email, systemID);
    if (systemAccessRole == api::SystemAccessRole::none)
        return completionHandler(api::ResultCode::forbidden, api::AuthResponse());

    api::AuthResponse response;
    response.nonce = authRequest.nonce;
    response.accessRole = systemAccessRole;
    const auto intermediateResponse = nx_http::calcIntermediateResponse(
        account->passwordHa1.c_str(),
        authRequest.nonce.c_str());
    response.intermediateResponse.assign(
        intermediateResponse.constData(),
        intermediateResponse.size());
    response.validPeriod = m_settings.auth().intermediateResponseValidityPeriod;

    completionHandler(api::ResultCode::ok, std::move(response));
}

bool AuthenticationProvider::parseNonce(
    const std::string& nonce,
    QByteArray* const random3Bytes,
    uint32_t* const timestamp,
    QByteArray* const nonceHash) const
{
    if (nonce.size() != CDB_NONCE_SIZE)
        return false;

    *random3Bytes = QByteArray(nonce.data(), 3);
    const QByteArray timestampAndHash = 
        QByteArray::fromBase64(QByteArray::fromRawData(nonce.data() + 3, nonce.size()-3));
    memcpy(timestamp, timestampAndHash.constData(), sizeof(*timestamp));
    *nonceHash = timestampAndHash.mid(sizeof(*timestamp));
    return true;
}

QByteArray AuthenticationProvider::calcNonceHash(
    const std::string& systemID,
    uint32_t timestamp) const
{
    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, systemID.c_str(), systemID.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, &timestamp, sizeof(timestamp));
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, SECRET_NONCE_KEY, strlen(SECRET_NONCE_KEY));
    QByteArray md5Hash;
    md5Hash.resize(MD5_DIGEST_LENGTH);
    MD5_Final(reinterpret_cast<unsigned char*>(md5Hash.data()), &md5Ctx);
    return md5Hash;
}

}   //cdb
}   //nx
