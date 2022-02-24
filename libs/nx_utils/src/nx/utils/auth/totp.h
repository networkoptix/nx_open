// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <openssl/hmac.h>
#include <QByteArray>

#include <nx/utils/type_utils.h>

namespace nx::utils::auth {

/**
 * Implements HOTP (https://tools.ietf.org/html/rfc4226) and TOTP (https://tools.ietf.org/html/rfc6238)
 * algorithms.
 */
class NX_UTILS_API TotpGenerator
{
public:
    TotpGenerator(std::chrono::seconds passwordUpdateInterval, int codeLength);

    int32_t getHotp(const std::string& key, int64_t counter) const;

    std::string getTotp(const std::string& key) const;

    bool validateTotp(const std::string& key, const std::string& totp, size_t counters) const;

    bool validateTotpRelaxed(const std::string& key, const std::string& totp) const;

    bool validateHotp(const std::string& key, int64_t counter, const std::string& hotp) const;

private:
    std::chrono::seconds m_passwordUpdateInterval;
    int m_codeLength = 6;

    int64_t getCurrentTimeValue() const;

    std::string serializeHotp(int32_t hotp) const;
};

} // namespace nx::utils::auth
