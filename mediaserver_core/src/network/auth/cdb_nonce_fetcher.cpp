/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cdb_nonce_fetcher.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "cloud/cloud_connection_manager.h"


namespace {
    constexpr const int kNonceTrailingRandomByteCount = 4;
    constexpr const char kMagicBytes[] = {'h', 'z'};
    constexpr const int kNonceTrailerLength =
        sizeof(kMagicBytes) + kNonceTrailingRandomByteCount;
    constexpr const std::chrono::seconds kGetNonceRetryTimeout = std::chrono::seconds(5);
}

CdbNonceFetcher::CdbNonceFetcher(std::unique_ptr<AbstractNonceProvider> defaultGenerator)
:
    m_defaultGenerator(std::move(defaultGenerator)),
    m_boundToCloud(false),
    m_randomEngine(m_rd()),
    m_nonceTrailerRandomGenerator('a', 'z')
{
    m_monotonicClock.restart();

    QnMutexLocker lk(&m_mutex);

    Qn::directConnect(
        CloudConnectionManager::instance(), &CloudConnectionManager::cloudBindingStatusChanged,
        this, &CdbNonceFetcher::cloudBindingStatusChanged);

    m_boundToCloud = CloudConnectionManager::instance()->boundToCloud();

    if (m_boundToCloud)
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            std::chrono::milliseconds::zero());
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
    QnMutexLocker lk(&m_mutex);

    if (m_boundToCloud)
    {
        const qint64 curClock = m_monotonicClock.elapsed();
        removeInvalidNonce(&m_cdbNonceQueue, curClock);

        if (!m_cdbNonceQueue.empty() &&
            m_cdbNonceQueue.back().expirationTime > curClock)
        {
            //we have valid cloud nonce
            QByteArray nonceTrailer;
            nonceTrailer.resize(kNonceTrailerLength);
            memcpy(nonceTrailer.data(), kMagicBytes, sizeof(kMagicBytes));
            std::generate(
                nonceTrailer.data()+sizeof(kMagicBytes),
                nonceTrailer.data()+nonceTrailer.size(),
                [&]{return m_nonceTrailerRandomGenerator(m_randomEngine);});
            const auto nonce = m_cdbNonceQueue.back().nonce + nonceTrailer;

            NX_LOGX(lm("Returning cloud nonce %1. Valid for another %2 sec")
                .arg(nonce).arg((m_cdbNonceQueue.back().expirationTime - curClock)/1000),
                cl_logDEBUG2);

            return nonce;
        }
        else
        {
            NX_LOGX(lit("No valid cloud nonce available..."), cl_logDEBUG2);
        }
    }

    lk.unlock();

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
    if ((nonce.size() > kNonceTrailerLength) &&
        (memcmp(
            nonce.constData() + nonce.size() - kNonceTrailerLength,
            kMagicBytes,
            sizeof(kMagicBytes)) == 0))
    {
        QnMutexLocker lk(&m_mutex);
        removeInvalidNonce(&m_cdbNonceQueue, m_monotonicClock.elapsed());
        if (!m_cdbNonceQueue.empty())
        {
            const auto cdbNonce = nonce.mid(0, nonce.size() - kNonceTrailerLength);
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
    if ((nonce.size() <= kNonceTrailerLength) ||
        (memcmp(
            nonce.constData() + nonce.size() - kNonceTrailerLength,
            kMagicBytes,
            sizeof(kMagicBytes)) != 0))
    {
        return false;
    }

    *cloudNonce = nonce.mid(0, nonce.size() - kNonceTrailerLength);
    *nonceTrailer = nonce.mid(nonce.size() - kNonceTrailerLength);
    return true;
}

void CdbNonceFetcher::fetchCdbNonceAsync()
{
    NX_LOGX(lm("Trying to fetch new cloud nonce"), cl_logDEBUG2);

    QnMutexLocker lk(&m_mutex);
    m_timerID.release();

    if (!m_boundToCloud)
        return;

    m_connection = CloudConnectionManager::instance()->getCloudConnection();
    if (!m_connection)
    {
        NX_LOG(lit("CdbNonceFetcher. Failed to get connection to cdb"), cl_logDEBUG1);
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            kGetNonceRetryTimeout);
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

    if (!m_boundToCloud)
        return;

    if (resCode != nx::cdb::api::ResultCode::ok)
    {
        NX_LOGX(lit("Failed to fetch nonce from cdb: %1").
            arg(static_cast<int>(resCode)), cl_logWARNING);
        CloudConnectionManager::instance()->processCloudErrorCode(resCode);
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            kGetNonceRetryTimeout);
        return;
    }

    using namespace std::chrono;

    const auto curTime = m_monotonicClock.elapsed();
    NonceCtx nonceCtx;
    nonceCtx.nonce = nonce.nonce.c_str();
    nonceCtx.validityTime =
        curTime +
        duration_cast<milliseconds>(nonce.validPeriod).count();
    nonceCtx.expirationTime =
        curTime +
        duration_cast<milliseconds>(nonce.validPeriod).count() / 2;

    NX_LOGX(lm("Got new cloud nonce %1, valid for another %2 sec")
        .arg(nonceCtx.nonce).arg((nonceCtx.expirationTime - curTime)/1000),
        cl_logDEBUG2);

    m_cdbNonceQueue.emplace_back(std::move(nonceCtx));

    m_timerID = nx::utils::TimerManager::instance()->addTimer(
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

void CdbNonceFetcher::cloudBindingStatusChanged(bool boundToCloud)
{
    NX_LOGX(lm("Cloud binding status changed: %1").arg(boundToCloud), cl_logDEBUG1);

    QnMutexLocker lk(&m_mutex);

    m_boundToCloud = boundToCloud;
    if (!boundToCloud)
    {
        m_cdbNonceQueue.clear();
        return;
    }

    if (m_timerID)
        nx::utils::TimerManager::instance()->modifyTimerDelay(
            m_timerID.get(),
            std::chrono::milliseconds::zero());
    else
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this),
            std::chrono::milliseconds::zero());
}
