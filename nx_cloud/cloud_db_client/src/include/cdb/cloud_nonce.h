#pragma once

#include <string>


namespace nx {
namespace cdb {
namespace api {

std::string calcNonceHash(
    const std::string& systemID,
    uint32_t timestamp);

/** Prepares nonce base to be used by anyone who wants to provide cloud 
    account credentials authentication (e.g. mediaserver). */
std::string generateCloudNonceBase(const std::string& systemID);

/** Returns \a true if \a nonce is a valid nonce generated with \a generateCloudNonce */
bool parseCloudNonceBase(
    const std::string& nonceBase,
    uint32_t* const timestamp,
    std::string* const nonceHash);

/** This method generates random nonce based on value returned by \a generateCloudNonce. */
std::string generateNonce(const std::string& cloudNonce);

}   // namespace api
}   // namespace cdb
}   // namespace nx
