#include "time_based_nonce_provider.h"

#include <nx/utils/log/log.h>

#include <utils/common/synctime.h>

static const qint64 NONCE_TIMEOUT = 1000000ll * 60 * 5;
static const qint64 COOKIE_EXPIRATION_PERIOD = 3600;

QByteArray TimeBasedNonceProvider::generateNonce()
{
    QnMutexLocker lock(&m_cookieNonceCacheMutex);
    const qint64 nonce = qnSyncTime->currentUSecsSinceEpoch();
    m_cookieNonceCache.insert(nonce, nonce);
    return QByteArray::number(nonce, 16);
}

bool TimeBasedNonceProvider::isNonceValid(const QByteArray& nonce) const
{
    if (nonce.isEmpty())
        return false;
    static const qint64 USEC_IN_SEC = 1000000ll;

    QnMutexLocker lock(&m_cookieNonceCacheMutex);

    const qint64 intNonce = nonce.toLongLong(0, 16);
    const qint64 curTimeUSec = qnSyncTime->currentUSecsSinceEpoch();

    bool rez;
    auto itr = m_cookieNonceCache.find(intNonce);
    if (itr == m_cookieNonceCache.end())
    {
        rez = qAbs(curTimeUSec - intNonce) < NONCE_TIMEOUT;
        if (rez)
            m_cookieNonceCache.insert(intNonce, curTimeUSec);
    }
    else
    {
        rez = curTimeUSec - itr.value() < COOKIE_EXPIRATION_PERIOD * USEC_IN_SEC;
        itr.value() = curTimeUSec;
    }

    // cleanup cookie cache

    const qint64 minAllowedTime = curTimeUSec - COOKIE_EXPIRATION_PERIOD * USEC_IN_SEC;
    for (auto itr = m_cookieNonceCache.begin(); itr != m_cookieNonceCache.end();)
    {
        if (itr.value() < minAllowedTime)
            itr = m_cookieNonceCache.erase(itr);
        else
            ++itr;
    }

    NX_VERBOSE(this, lm("Nonce %1 is %2").arg(nonce).arg(rez ? "valid" : "not valid"));

    return rez;
}
