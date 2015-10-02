/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cdb_nonce_fetcher.h"

#include <algorithm>

#include <utils/common/log.h>

#include "cloud/cloud_connection_manager.h"


namespace {
    const int NONCE_TRAILING_RANDOM_BYTES = 4;
    const char MAGIC_BYTES[] = {'h', 'z'};
    const int NONCE_TRAILER_LEN = sizeof(MAGIC_BYTES) + NONCE_TRAILING_RANDOM_BYTES;
    const std::chrono::seconds GET_NONCE_RETRY_TIMEOUT(5);
}

CdbNonceFetcher::CdbNonceFetcher(std::unique_ptr<AbstractNonceProvider> defaultGenerator)
:
    m_defaultGenerator(std::move(defaultGenerator)),
    m_bindedToCloud(false)
{
    m_monotonicClock.restart();

    connect(CloudConnectionManager::instance(), &CloudConnectionManager::cloudBindingStatusChanged,
            this, &CdbNonceFetcher::cloudBindingStatusChanged);
    m_timerID = TimerManager::instance()->addTimer(
        std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
        0);
}

CdbNonceFetcher::~CdbNonceFetcher()
{
    QnMutexLocker lk(&m_mutex);
    auto connection = std::move(m_connection);
    auto timerID = std::move(m_timerID);
    lk.unlock();

    connection.reset();
    timerID.reset();
}

QByteArray CdbNonceFetcher::generateNonce()
{
    {
        QnMutexLocker lk(&m_mutex);

        removeExpiredNonce(&lk);

        if (!m_cdbNonceQueue.empty() &&
            m_cdbNonceQueue.back().validityTime > m_monotonicClock.elapsed())
        {
            //we have valid cloud nonce
            QByteArray nonceTrailer;
            nonceTrailer.resize(NONCE_TRAILER_LEN);
            memcpy(nonceTrailer.data(), MAGIC_BYTES, sizeof(MAGIC_BYTES));
            std::generate(
                nonceTrailer.data()+sizeof(MAGIC_BYTES),
                nonceTrailer.data()+nonceTrailer.size(),
                rand);
            const auto nonce = m_cdbNonceQueue.back().nonce + nonceTrailer;
            return nonce;
        }
    }

    return m_defaultGenerator->generateNonce();
}

bool CdbNonceFetcher::isNonceValid(const QByteArray& nonce) const
{
    if ((nonce.size() > NONCE_TRAILER_LEN) &&
        (memcmp(
            nonce.constData()+nonce.size()-NONCE_TRAILER_LEN,
            MAGIC_BYTES,
            sizeof(MAGIC_BYTES)) == 0))
    {
        QnMutexLocker lk(&m_mutex);
        removeExpiredNonce(&lk);
        if (!m_cdbNonceQueue.empty())
        {
            const auto cdbNonce = nonce.mid(0, nonce.size() - NONCE_TRAILER_LEN);
            for (const auto& nonceCtx: m_cdbNonceQueue)
                if (nonceCtx.nonce == cdbNonce)
                    return true;
        }
    }

    return m_defaultGenerator->isNonceValid(nonce);
}

void CdbNonceFetcher::fetchCdbNonceAsync()
{
    QnMutexLocker lk(&m_mutex);
    m_timerID.release();

    m_connection = CloudConnectionManager::instance()->getCloudConnection();
    if (!m_connection)
    {
        NX_LOG(lit("CdbNonceFetcher. Failed to get connection to cdb"), cl_logDEBUG1);
        return;
    }

    using namespace std::placeholders;
    m_connection->authProvider()->getCdbNonce(
        std::bind(&CdbNonceFetcher::gotNonce, this, _1, _2));
}

void CdbNonceFetcher::gotNonce(
    nx::cdb::api::ResultCode resCode,
    nx::cdb::api::NonceData nonce)
{
    QnMutexLocker lk(&m_mutex);

    using namespace std::placeholders;

    if (resCode != nx::cdb::api::ResultCode::ok)
    {
        NX_LOG(lit("CdbNonceFetcher. Failed to fetch nonce from cdb: %1").
            arg(static_cast<int>(resCode)), cl_logWARNING);
        m_timerID = TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            GET_NONCE_RETRY_TIMEOUT);
        return;
    }

    const auto curTime = m_monotonicClock.elapsed();
    NonceCtx nonceCtx;
    nonceCtx.nonce = nonce.nonce.c_str();
    nonceCtx.expirationTime =
        curTime +
        std::chrono::duration_cast<std::chrono::milliseconds>(nonce.validPeriod).count();
    nonceCtx.validityTime =
        curTime +
        std::chrono::duration_cast<std::chrono::milliseconds>(nonce.validPeriod).count() / 2;
        
    m_cdbNonceQueue.emplace_back(std::move(nonceCtx));

    m_timerID = TimerManager::instance()->addTimer(
        std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
        nonce.validPeriod/2);
}

void CdbNonceFetcher::removeExpiredNonce(QnMutexLockerBase* const /*lk*/)
{
    while (!m_cdbNonceQueue.empty() &&
           m_cdbNonceQueue.front().expirationTime < m_monotonicClock.elapsed())
    {
        m_cdbNonceQueue.pop_front();
    }
}

void CdbNonceFetcher::cloudBindingStatusChanged(bool bindedToCloud)
{
    if (!bindedToCloud)
        return;

    QnMutexLocker lk(&m_mutex);
    if (m_timerID)
        TimerManager::instance()->modifyTimerDelay(m_timerID.get(), 0);
    else
        m_timerID = TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            0);
}
