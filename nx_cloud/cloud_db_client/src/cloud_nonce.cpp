
#include "include/cdb/cloud_nonce.h"

#include <chrono>

#include <openssl/md5.h>

#include <nx/utils/random.h>


namespace nx {
namespace cdb {
namespace api {

namespace {
static const char kSecretNonceKey[] = "neurod.ru";
static const size_t kCdbNonceSize = 31;

const constexpr int kRandomBytesCount = 3;
const constexpr int kFullNonceTrailerSize = 3;

constexpr const auto kNonceTrailingRandomByteCount = 4;
constexpr const char kMagicBytes[] = {'h', 'z'};
constexpr const auto kNonceTrailerLength =
    sizeof(kMagicBytes) + kNonceTrailingRandomByteCount;

void calcNonceHash(
    const std::string& systemID,
    uint32_t timestamp,
    char* md5HashBuf)
{
    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, systemID.c_str(), systemID.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, &timestamp, sizeof(timestamp));
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, kSecretNonceKey, strlen(kSecretNonceKey));
    MD5_Final(reinterpret_cast<unsigned char*>(md5HashBuf), &md5Ctx);
}
}   // namespace

std::string calcNonceHash(
    const std::string& systemID,
    uint32_t timestamp)
{
    std::string nonceHash;
    nonceHash.resize(MD5_DIGEST_LENGTH);
    calcNonceHash(systemID, timestamp, &nonceHash[0]);
    return nonceHash;
}

std::string generateCloudNonceBase(const std::string& systemID)
{
    //{random_3_bytes}base64({ timestamp }MD5(systemID:timestamp:secret_key))

    const uint32_t timestamp = std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    //TODO #ak replace with proper vectors function when available
    char randomBytes[kRandomBytesCount+1];
    for (int i = 0; i < kRandomBytesCount; ++i)
        randomBytes[i] = nx::utils::random::number<int>('a', 'z');
    randomBytes[kRandomBytesCount] = '\0';

    QByteArray md5Hash;
    md5Hash.resize(MD5_DIGEST_LENGTH);
    calcNonceHash(systemID, timestamp, md5Hash.data());

    //TODO #ak timestamp byte order

    QByteArray nonce =
        randomBytes +
        (QByteArray::fromRawData(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp))
            + md5Hash).toBase64();

    return nonce.constData();
}

bool parseCloudNonceBase(
    const std::string& nonceBase,
    uint32_t* const timestamp,
    std::string* const nonceHash)
{
    if (nonceBase.size() != kCdbNonceSize)
        return false;

    const QByteArray timestampAndHash =
        QByteArray::fromBase64(QByteArray::fromRawData(
            nonceBase.data() + kRandomBytesCount,
            nonceBase.size() - kRandomBytesCount));
    //TODO #ak timestamp byte order
    memcpy(timestamp, timestampAndHash.constData(), sizeof(*timestamp));
    *nonceHash = timestampAndHash.constData() + sizeof(*timestamp);
    return true;
}

std::string generateNonce(const std::string& cloudNonce)
{
    //if request is authenticated with account permissions, returning full nonce
    //  TODO #ak some refactor in mediaserver and here is required to remove this condition at all
    //nonce = ;
    std::string nonce;
    nonce.resize(cloudNonce.size() + kNonceTrailerLength);
    memcpy(&nonce[0], cloudNonce.data(), cloudNonce.size());
    std::string::size_type noncePos = cloudNonce.size();

    memcpy(&nonce[noncePos], kMagicBytes, sizeof(kMagicBytes));
    noncePos += sizeof(kMagicBytes);

    //adding trailing random bytes
    for (; noncePos < nonce.size(); ++noncePos)
        nonce[noncePos] = nx::utils::random::number<int>('a', 'z');

    return nonce;
}

}   // namespace api
}   // namespace cdb
}   // namespace nx
