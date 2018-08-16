#pragma once

#include <string>

namespace nx {
namespace cdb {
namespace api {

/**
 * @param timestamp This value is converted to network byte order to calculate hash
 * @return Raw hash (it may contain unprintable symbols including \0)
 */
std::string calcNonceHash(
    const std::string& systemId,
    uint32_t timestamp);

/**
 * Prepares nonce base to be used by anyone who wants to provide cloud
 * account credentials authentication (e.g. mediaserver).
 */
std::string generateCloudNonceBase(const std::string& systemId);

/**
 * @param timestamp Returned value is always in local byte order
 * @param nonceHash Raw hash (it may contain unprintable symbols including \0)
 * @return true if nonce is a valid nonce generated with generateCloudNonce.
 */
bool parseCloudNonceBase(
    const std::string& nonceBase,
    uint32_t* const timestamp,
    std::string* const nonceHash);

bool isValidCloudNonceBase(
    const std::string& suggestedNonceBase,
    const std::string& systemId);

/**
 * Generates random nonce based on value returned by generateCloudNonce.
 */
std::string generateNonce(const std::string& cloudNonce);

bool isNonceValidForSystem(
    const std::string& nonce,
    const std::string& systemId);

} // namespace api
} // namespace cdb
} // namespace nx
