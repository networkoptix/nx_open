/**********************************************************
* Sep 28, 2015
* a.kolesnikov
***********************************************************/

#include "auth_provider.h"

#include <chrono>

#include <openssl/md5.h>

#include <utils/common/uuid.h>
#include <utils/common/util.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {

static const char SECRET_NONCE_KEY[] = "neurod.ru";
static const std::chrono::minutes NONCE_VALIDITY_PERIOD(5);

void AuthenticationProvider::getCdbNonce(
    const AuthorizationInfo& authzInfo,
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    QnUuid systemID;
    if (!authzInfo.get(attr::systemID, &systemID))
    {
        completionHandler(api::ResultCode::forbidden, api::NonceData());
        return;
    }

    //{random_3_bytes}base64({ timestamp }MD5(systemID:timestamp:secret_key))

    const uint32_t timestamp = std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    const auto systemIDStr = systemID.toByteArray();
    MD5_Update(&md5Ctx, systemIDStr.constData(), systemIDStr.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, &timestamp, sizeof(timestamp));
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, SECRET_NONCE_KEY, strlen(SECRET_NONCE_KEY));
    QByteArray md5Hash;
    md5Hash.resize(MD5_DIGEST_LENGTH);
    MD5_Final(reinterpret_cast<unsigned char*>(md5Hash.data()), &md5Ctx);

    QByteArray random3Bytes;
    random3Bytes.resize(3);
    for(auto& ch: random3Bytes)
        ch = random('a', 'z');
    const auto nonce =
        random3Bytes +
        (QByteArray::fromRawData(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp))
            +md5Hash).toBase64();

    assert(nonce.size() == 31);

    api::NonceData result;
    result.nonce.assign(nonce.constData(), nonce.size());
    result.validPeriod = NONCE_VALIDITY_PERIOD;

    completionHandler(api::ResultCode::ok, std::move(result));
}

void AuthenticationProvider::getAuthenticationResponse(
    const AuthorizationInfo& authzInfo,
    const api::AuthRequest& authRequest,
    std::function<void(api::ResultCode, api::AuthResponse)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented, api::AuthResponse());
}

}   //cdb
}   //nx
