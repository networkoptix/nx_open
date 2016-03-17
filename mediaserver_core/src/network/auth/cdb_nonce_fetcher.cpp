/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cdb_nonce_fetcher.h"

#include <algorithm>

#include <nx/utils/log/log.h>

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
    m_bindedToCloud(false),
    m_randomEngine(m_rd()),
    m_nonceTrailerRandomGenerator('a', 'z')
{
    m_monotonicClock.restart();

    Qn::directConnect(
        CloudConnectionManager::instance(), &CloudConnectionManager::cloudBindingStatusChanged,
        this, &CdbNonceFetcher::cloudBindingStatusChanged);
    m_timerID = TimerManager::instance()->addTimer(
        std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
        0);
}

CdbNonceFetcher::~CdbNonceFetcher()
{
    directDisconnectAll();

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

        const qint64 curClock = m_monotonicClock.elapsed();
        removeInvalidNonce(&m_cdbNonceQueue, curClock);

        if (!m_cdbNonceQueue.empty() &&
            m_cdbNonceQueue.back().expirationTime > curClock)
        {
            //we have valid cloud nonce
            QByteArray nonceTrailer;
            nonceTrailer.resize(NONCE_TRAILER_LEN);
            memcpy(nonceTrailer.data(), MAGIC_BYTES, sizeof(MAGIC_BYTES));
            std::generate(
                nonceTrailer.data()+sizeof(MAGIC_BYTES),
                nonceTrailer.data()+nonceTrailer.size(),
                [&]{return m_nonceTrailerRandomGenerator(m_randomEngine);});
            const auto nonce = m_cdbNonceQueue.back().nonce + nonceTrailer;
            return nonce;
        }
    }

    return m_defaultGenerator->generateNonce();
}

bool CdbNonceFetcher::isNonceValid(const QByteArray& nonce) const
{
    if (isValidCloudNonce(nonce))
        return true;
    return m_defaultGenerator->isNonceValid(nonce);
}

bool CdbNonceFetcher::isValidCloudNonce(const QByteArray& nonce) const
{
    if ((nonce.size() > NONCE_TRAILER_LEN) &&
        (memcmp(
            nonce.constData() + nonce.size() - NONCE_TRAILER_LEN,
            MAGIC_BYTES,
            sizeof(MAGIC_BYTES)) == 0))
    {
        QnMutexLocker lk(&m_mutex);
        removeInvalidNonce(&m_cdbNonceQueue, m_monotonicClock.elapsed());
        if (!m_cdbNonceQueue.empty())
        {
            const auto cdbNonce = nonce.mid(0, nonce.size() - NONCE_TRAILER_LEN);
            for (const auto& nonceCtx : m_cdbNonceQueue)
                if (nonceCtx.nonce == cdbNonce)
                    return true;
        }
    }
    return false;
}

bool CdbNonceFetcher::parseCloudNonce(
    const nx_http::BufferType& nonce,
    nx_http::BufferType* const cloudNonce,
    nx_http::BufferType* const nonceTrailer)
{
    if ((nonce.size() <= NONCE_TRAILER_LEN) ||
        (memcmp(
            nonce.constData() + nonce.size() - NONCE_TRAILER_LEN,
            MAGIC_BYTES,
            sizeof(MAGIC_BYTES)) != 0))
    {
        return false;
    }

    *cloudNonce = nonce.mid(0, nonce.size() - NONCE_TRAILER_LEN);
    *nonceTrailer = nonce.mid(nonce.size() - NONCE_TRAILER_LEN);
    return true;
}

void CdbNonceFetcher::fetchCdbNonceAsync()
{
    QnMutexLocker lk(&m_mutex);
    m_timerID.release();

    m_connection = CloudConnectionManager::instance()->getCloudConnection();
    if (!m_connection)
    {
        NX_LOG(lit("CdbNonceFetcher. Failed to get connection to cdb"), cl_logDEBUG1);
        m_timerID = TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            GET_NONCE_RETRY_TIMEOUT);
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
    nonceCtx.validityTime =
        curTime +
        std::chrono::duration_cast<std::chrono::milliseconds>(nonce.validPeriod).count();
    nonceCtx.expirationTime =
        curTime +
        std::chrono::duration_cast<std::chrono::milliseconds>(nonce.validPeriod).count() / 2;
        
    m_cdbNonceQueue.emplace_back(std::move(nonceCtx));

    m_timerID = TimerManager::instance()->addTimer(
        std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
        nonce.validPeriod/2);
}

void CdbNonceFetcher::removeInvalidNonce(
    std::deque<NonceCtx>* const cdbNonceQueue,
    qint64 curClock)
{
    while (!cdbNonceQueue->empty() &&
           cdbNonceQueue->front().validityTime < curClock)
    {
        cdbNonceQueue->pop_front();
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
