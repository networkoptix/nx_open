// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <sstream>
#include <iomanip>

#include "totp.h"

#include <QMessageAuthenticationCode>

#include <nx/utils/time.h>

#include "utils.h"

namespace nx::utils::auth {

TotpGenerator::TotpGenerator(std::chrono::seconds passwordUpdateInterval, int codeLength):
    m_passwordUpdateInterval(passwordUpdateInterval), m_codeLength(codeLength)
{
}

std::string TotpGenerator::getTotp(const std::string& key) const
{
    const int64_t timeValue = getCurrentTimeValue();
    int hotp = getHotp(key, timeValue);
    return serializeHotp(hotp);
}

bool TotpGenerator::validateTotp(
    const std::string& key, const std::string& totp, size_t counters) const
{
    const int64_t timeValue = getCurrentTimeValue();
    bool isValid = validateHotp(key, timeValue, totp);
    for (size_t i = 1; i < counters; ++i)
    {
        isValid |= validateHotp(key, timeValue - i, totp);
        isValid |= validateHotp(key, timeValue + i, totp);
    }
    return isValid;
}

bool TotpGenerator::validateTotpRelaxed(const std::string& key, const std::string& totp) const
{
    return validateTotp(key, totp, 2);
}

bool TotpGenerator::validateHotp(const std::string& key, int64_t counter,
    const std::string& hotp) const
{
    int calculatedHotp = getHotp(key, counter);
    return serializeHotp(calculatedHotp) == hotp;
}

int32_t TotpGenerator::getHotp(const std::string& key, int64_t counter) const
{
    const int counterLength = 8;
    auto counterStr = std::string(counterLength, 0);
    for (int i = counterLength - 1; i >= 0; --i)
    {
        counterStr[i] = counter & 0xFF;
        counter = counter >> 8;
    }
    const auto hmacResult = hmacSha1(key, counterStr);
    const int offset = hmacResult[19] & 0xf;
    const int bin_code = ((hmacResult[offset] & 0x7f) << 24)
        | (hmacResult[offset + 1] & 0xFF) << 16 | (hmacResult[offset + 2] & 0xFF) << 8
        | (hmacResult[offset + 3] & 0xFF);
    return bin_code % (int) std::pow(10, m_codeLength);
}

int64_t TotpGenerator::getCurrentTimeValue() const
{
    const auto curTime =
        std::chrono::duration_cast<std::chrono::seconds>(nx::utils::utcTime().time_since_epoch())
            .count();
    return curTime / m_passwordUpdateInterval.count();
}

std::string TotpGenerator::serializeHotp(int32_t hotp) const
{
    std::stringstream ss;
    ss << std::setw(m_codeLength) << std::setfill('0') << hotp;
    return ss.str();
}

} // namespace nx::utils::auth
