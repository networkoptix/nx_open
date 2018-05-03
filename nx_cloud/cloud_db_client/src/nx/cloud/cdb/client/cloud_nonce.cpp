#include "include/nx/cloud/cdb/api/cloud_nonce.h"

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <chrono>

#include <openssl/md5.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/cryptographic_random_device.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>

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
    const std::string& systemId,
    uint32_t timestamp,
    char* md5HashBuf)
{
    const uint32_t timestampInNetworkByteOrder = htonl(timestamp);

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, systemId.c_str(), systemId.size());
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, &timestampInNetworkByteOrder, sizeof(timestampInNetworkByteOrder));
    MD5_Update(&md5Ctx, ":", 1);
    MD5_Update(&md5Ctx, kSecretNonceKey, strlen(kSecretNonceKey));
    MD5_Final(reinterpret_cast<unsigned char*>(md5HashBuf), &md5Ctx);
}

}   // namespace

std::string calcNonceHash(
    const std::string& systemId,
    uint32_t timestamp)
{
    std::string nonceHash;
    nonceHash.resize(MD5_DIGEST_LENGTH);
    calcNonceHash(systemId, timestamp, &nonceHash[0]);
    return nonceHash;
}

std::string generateCloudNonceBase(const std::string& systemId)
{
    //{random_3_bytes}base64({ timestamp }MD5(systemId:timestamp:secret_key))

    const uint32_t timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::timeSinceEpoch()).count();
    const uint32_t timestampInNetworkByteOrder = htonl(timestamp);

    // TODO: #ak Replace with proper vector function when available.
    char randomBytes[kRandomBytesCount+1];
    for (int i = 0; i < kRandomBytesCount; ++i)
    {
        randomBytes[i] = nx::utils::random::number(
            nx::utils::random::CryptographicRandomDevice::instance(),
            (int)'a', (int)'z');
    }
    randomBytes[kRandomBytesCount] = '\0';

    QByteArray md5Hash;
    md5Hash.resize(MD5_DIGEST_LENGTH);
    calcNonceHash(systemId, timestamp, md5Hash.data());

    const auto timestampInNetworkByteOrderBuf =
        QByteArray::fromRawData(
            reinterpret_cast<const char*>(&timestampInNetworkByteOrder),
            sizeof(timestampInNetworkByteOrder));
    QByteArray nonce =
        QByteArray(randomBytes)
        + (timestampInNetworkByteOrderBuf + md5Hash).toBase64();

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
    memcpy(timestamp, timestampAndHash.constData(), sizeof(*timestamp));
    *timestamp = ntohl(*timestamp);
    NX_ASSERT(timestampAndHash.size() - sizeof(*timestamp) == MD5_DIGEST_LENGTH);
    nonceHash->resize(MD5_DIGEST_LENGTH);
    memcpy(
        &nonceHash->at(0),
        timestampAndHash.constData() + sizeof(*timestamp),
        MD5_DIGEST_LENGTH);
    return true;
}

bool isValidCloudNonceBase(
    const std::string& suggestedNonceBase,
    const std::string& systemId)
{
    uint32_t timestamp = 0;
    std::string nonceHash;
    if (!parseCloudNonceBase(suggestedNonceBase, &timestamp, &nonceHash))
        return false;

    return nonceHash == calcNonceHash(systemId, timestamp);
}

std::string generateNonce(const std::string& cloudNonce)
{
    std::string nonce;
    nonce.resize(cloudNonce.size() + kNonceTrailerLength);
    memcpy(&nonce[0], cloudNonce.data(), cloudNonce.size());
    std::string::size_type noncePos = cloudNonce.size();

    memcpy(&nonce[noncePos], kMagicBytes, sizeof(kMagicBytes));
    noncePos += sizeof(kMagicBytes);

    // Adding trailing random bytes.
    for (; noncePos < nonce.size(); ++noncePos)
    {
        nonce[noncePos] = nx::utils::random::number(
            nx::utils::random::CryptographicRandomDevice::instance(),
            (int)'a', (int)'z');
    }

    return nonce;
}

bool isNonceValidForSystem(
    const std::string& nonce,
    const std::string& systemId)
{
    if (nonce.size() <= kNonceTrailerLength)
        return false;

    return isValidCloudNonceBase(
        nonce.substr(0, nonce.size() - kNonceTrailerLength),
        systemId);
}

} // namespace api
} // namespace cdb
} // namespace nx
