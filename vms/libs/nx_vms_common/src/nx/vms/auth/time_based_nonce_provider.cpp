// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_based_nonce_provider.h"

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

static const std::chrono::minutes kNonceServerTimeDifference(5);
static const std::chrono::hours kNonceExpirationPeriod(1);

TimeBasedNonceProvider::TimeBasedNonceProvider(
    std::chrono::milliseconds maxServerTimeDifference,
    std::chrono::milliseconds steadyExpirationPeriod)
:
    m_maxServerTimeDifference(maxServerTimeDifference),
    m_steadyExpirationPeriod(steadyExpirationPeriod)
{
    NX_DEBUG(this, nx::format("Server time difference %1, steady expiration period %2").args(
        m_maxServerTimeDifference, m_steadyExpirationPeriod));
}

nx::String TimeBasedNonceProvider::generateNonce()
{
    const auto nonceTime = qnSyncTime->currentTimePoint();
    const auto nonce = generateTimeBasedNonce(nonceTime);
    NX_VERBOSE(this, nx::format("Generated %1 (%2)").args(nonce, nonceTime));

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_nonceCache.emplace(nonceTime, std::chrono::steady_clock::now());
    return nonce;
}

nx::String TimeBasedNonceProvider::generateTimeBasedNonce(std::chrono::microseconds nonceTime)
{
    if (nonceTime == std::chrono::microseconds::zero())
        nonceTime = qnSyncTime->currentTimePoint();
    const auto nonce = nx::String::number((qint64)nonceTime.count(), 16);
    return nonce;
}

bool TimeBasedNonceProvider::isNonceValid(const nx::String& nonce) const
{
    bool isOk = false;
    const NonceTime nonceTime(nonce.toLongLong(&isOk, 16));
    if (!isOk)
        return false;

    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto steadyTime = std::chrono::steady_clock::now();
    for (auto it = m_nonceCache.begin(); it != m_nonceCache.end(); )
    {
        if (it->second + m_steadyExpirationPeriod < steadyTime)
            it = m_nonceCache.erase(it);
        else
            ++it;
    }

    const auto cacheRecord = m_nonceCache.find(nonceTime);
    if (cacheRecord != m_nonceCache.end())
    {
        NX_VERBOSE(this, nx::format("Prolong known %1 (%2)").args(nonce, nonceTime));
        cacheRecord->second = steadyTime;
        return true;
    }

    const auto currentServerTime = qnSyncTime->currentTimePoint();
    if (nonceTime > currentServerTime - m_maxServerTimeDifference &&
        nonceTime < currentServerTime + m_maxServerTimeDifference)
    {
        NX_VERBOSE(this, nx::format("Save close server time %1 (%2)").args(nonce, nonceTime));
        m_nonceCache.emplace(nonceTime, steadyTime);
        return true;
    }

    NX_VERBOSE(this, nx::format("Reject invalid %1 (%2)").args(nonce, nonceTime));
    return false;
}
