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
    NX_DEBUG(this, lm("Server time difference %1, steady expiration period %2").args(
        m_maxServerTimeDifference, m_steadyExpirationPeriod));
}

QByteArray TimeBasedNonceProvider::generateNonce()
{
    const auto nonceTime = qnSyncTime->currentTimePoint();
    const auto nonce = QByteArray::number((qint64) nonceTime.count(), 16);
    NX_VERBOSE(this, lm("Generated %1 (%2)").args(nonce, nonceTime));

    QnMutexLocker lock(&m_mutex);
    m_nonceCache.emplace(nonceTime, std::chrono::steady_clock::now());
    return nonce;
}

bool TimeBasedNonceProvider::isNonceValid(const QByteArray& nonce) const
{
    bool isOk = false;
    const NonceTime nonceTime(nonce.toLongLong(&isOk, 16));
    if (!isOk)
        return false;

    QnMutexLocker lock(&m_mutex);
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
        NX_VERBOSE(this, lm("Prolong known %1 (%2)").args(nonce, nonceTime));
        cacheRecord->second = steadyTime;
        return true;
    }

    const auto currentServerTime = qnSyncTime->currentTimePoint();
    if (nonceTime > currentServerTime - m_maxServerTimeDifference &&
        nonceTime < currentServerTime + m_maxServerTimeDifference)
    {
        NX_VERBOSE(this, lm("Save close server time %1 (%2)").args(nonce, nonceTime));
        m_nonceCache.emplace(nonceTime, steadyTime);
        return true;
    }

    NX_VERBOSE(this, lm("Reject invalid %1 (%2)").args(nonce, nonceTime));
    return false;
}
