#include "cdb_nonce_fetcher.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>

#include "cloud_connection_manager.h"

namespace {

constexpr const int kNonceTrailingRandomByteCount = 4;
constexpr const char kMagicBytes[] = {'h', 'z'};
constexpr const int kNonceTrailerLength = sizeof(kMagicBytes) + kNonceTrailingRandomByteCount;
constexpr const std::chrono::seconds kGetNonceRetryTimeout = std::chrono::minutes(1);

} // namespace

namespace nx {
namespace vms {
namespace cloud_integration {

CdbNonceFetcher::CdbNonceFetcher(
    AbstractCloudConnectionManager* const cloudConnectionManager,
    AbstractCloudUserInfoPool* cloudUserInfoPool,
    auth::AbstractNonceProvider* defaultGenerator)
:
    m_cloudConnectionManager(cloudConnectionManager),
    m_defaultGenerator(defaultGenerator),
    m_cloudUserInfoPool(cloudUserInfoPool)
{
    m_monotonicClock.restart();

    QnMutexLocker lock(&m_mutex);

    Qn::directConnect(
        m_cloudConnectionManager, &CloudConnectionManager::cloudBindingStatusChanged,
        this, &CdbNonceFetcher::cloudBindingStatusChanged);

    if (m_cloudConnectionManager->boundToCloud())
        m_timer.post(std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this));
}

CdbNonceFetcher::~CdbNonceFetcher()
{
    directDisconnectAll();

    m_timer.executeInAioThreadSync(
        [this]()
        {
            m_timer.pleaseStopSync();
            m_connection.reset();
        });
}

nx::Buffer CdbNonceFetcher::generateNonceTrailer()
{
    nx::Buffer nonceTrailer;
    nonceTrailer.resize(kNonceTrailerLength);
    memcpy(nonceTrailer.data(), kMagicBytes, sizeof(kMagicBytes));
    std::generate(
        nonceTrailer.data()+sizeof(kMagicBytes),
        nonceTrailer.data()+nonceTrailer.size(),
        []() { return nx::utils::random::number<int>('a', 'z'); });
    return nonceTrailer;
}

QByteArray CdbNonceFetcher::generateNonce()
{
    auto cloudPreviouslyProvidedNonce = m_cloudUserInfoPool->newestMostCommonNonce();
    if (cloudPreviouslyProvidedNonce)
        return *cloudPreviouslyProvidedNonce + generateNonceTrailer();

    if (!m_cloudConnectionManager->boundToCloud())
        return m_defaultGenerator->generateNonce();

    {
        QnMutexLocker lock(&m_mutex);

        const qint64 curClock = m_monotonicClock.elapsed();
        removeExpiredNonce(lock, curClock);

        if (!m_cdbNonceQueue.empty() &&
            m_cdbNonceQueue.back().expirationTime > curClock)
        {
            //we have valid cloud nonce
            const auto nonce = m_cdbNonceQueue.back().nonce + generateNonceTrailer();

            NX_VERBOSE(this, lm("Returning cloud nonce %1. Valid for another %2 sec")
                .arg(nonce).arg((m_cdbNonceQueue.back().expirationTime - curClock)/1000));

            return nonce;
        }
        else
        {
            NX_VERBOSE(this, lit("No valid cloud nonce available..."));
        }
    }

    return m_defaultGenerator->generateNonce();
}

bool CdbNonceFetcher::isNonceValid(const QByteArray& nonce) const
{
    NX_VERBOSE(this, lm("Verifying nonce %1").arg(nonce));
    if (isValidCloudNonce(nonce))
    {
        NX_VERBOSE(this, lm("Nonce %1 is a valid cloud nonce").arg(nonce));
        return true;
    }
    NX_VERBOSE(this, lm("Passing nonce %1 verification to a default verificator").arg(nonce));
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
        const std::string cloudNonceBase(nonce.constData(), nonce.size() - kNonceTrailerLength);
        auto cloudPreviouslyProvidedNonce = m_cloudUserInfoPool->newestMostCommonNonce();
        if (cloudPreviouslyProvidedNonce && nonce.startsWith(*cloudPreviouslyProvidedNonce))
        {
            NX_VERBOSE(this, lm("Approving nonce %1 since it has been found in cloud user pool")
                .args(nonce));
            return true;
        }

        const auto cloudSystemCredentials = m_cloudConnectionManager->getSystemCredentials();
        if (!cloudSystemCredentials)
        {
            NX_VERBOSE(this, lm("Rejecting nonce %1 since no cloud system id known").args(nonce));
            return false;   //we can't say if that nonce is ok for us
        }

        if (nx::cdb::api::isValidCloudNonceBase(
                cloudNonceBase,
                cloudSystemCredentials->systemId.constData()))
        {
            NX_VERBOSE(this, lm("Approving nonce %1 as a valid cloud nonce for cloud system %2")
                .args(nonce, cloudSystemCredentials->systemId));
            return true;
        }
        else
        {
            NX_VERBOSE(this, lm("Rejecting nonce %1 as invalid for cloud system %2")
                .args(nonce, cloudSystemCredentials->systemId));
            return false;
        }
    }

    NX_VERBOSE(this, "Nonce size < trailer size or trailer is not magic");
    return false;
}

nx::cdb::api::ResultCode CdbNonceFetcher::initializeConnectionToCloudSync()
{
    using namespace nx::cdb::api;

    auto newConnection = m_cloudConnectionManager->getCloudConnection();
    NX_ASSERT(newConnection);

    ResultCode resultCode = ResultCode::ok;
    NonceData cloudNonce;
    std::tie(resultCode, cloudNonce) = makeSyncCall<ResultCode, NonceData>(
        static_cast<void(AuthProvider::*)(std::function<void(ResultCode, NonceData)>)>(
            &AuthProvider::getCdbNonce),
        newConnection->authProvider(),
        std::placeholders::_1);

    if (resultCode != ResultCode::ok)
        return resultCode;

    QnMutexLocker lock(&m_mutex);

    cloudBindingStatusChangedUnsafe(lock, true);
    saveCloudNonce(std::move(cloudNonce));

    return ResultCode::ok;
}

bool CdbNonceFetcher::parseCloudNonce(
    const nx::network::http::BufferType& nonce,
    nx::network::http::BufferType* const cloudNonce,
    nx::network::http::BufferType* const nonceTrailer)
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
    NX_ASSERT(m_timer.isInSelfAioThread());
    m_timer.cancelSync();

    NX_VERBOSE(this, lm("Trying to fetch new cloud nonce"));

    std::unique_ptr<nx::cdb::api::Connection> newConnection;

    QnMutexLocker lock(&m_mutex);

    newConnection = m_cloudConnectionManager->getCloudConnection();
    std::swap(m_connection, newConnection);
    if (!m_connection)
    {
        NX_DEBUG(this, lit("Failed to get connection to cdb"));
        m_timer.start(
            kGetNonceRetryTimeout,
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this));
        return;
    }

    using namespace std::placeholders;
    m_connection->bindToAioThread(m_timer.getAioThread());
    m_connection->authProvider()->getCdbNonce(
        std::bind(&CdbNonceFetcher::gotNonce, this, _1, _2));
}

void CdbNonceFetcher::removeExpiredNonce(
    const QnMutexLockerBase& /*lock*/,
    qint64 curClock)
{
    while (!m_cdbNonceQueue.empty() &&
        m_cdbNonceQueue.front().validityTime < curClock)
    {
        m_cdbNonceQueue.pop_front();
    }
}

void CdbNonceFetcher::gotNonce(
    nx::cdb::api::ResultCode resCode,
    nx::cdb::api::NonceData nonce)
{
    NX_ASSERT(m_timer.isInSelfAioThread());

    if (!m_cloudConnectionManager->boundToCloud())
        return;

    if (resCode != nx::cdb::api::ResultCode::ok)
    {
        NX_WARNING(this, lit("Failed to fetch nonce from cdb: %1").
            arg(static_cast<int>(resCode)));
        m_cloudConnectionManager->processCloudErrorCode(resCode);
        m_timer.start(
            kGetNonceRetryTimeout,
            std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this));
        return;
    }

    QnMutexLocker lock(&m_mutex);

    saveCloudNonce(std::move(nonce));

    m_timer.start(
        nonce.validPeriod / 2,
        std::bind(&CdbNonceFetcher::fetchCdbNonceAsync, this));
}

void CdbNonceFetcher::saveCloudNonce(nx::cdb::api::NonceData nonce)
{
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

    NX_VERBOSE(this, lm("Got new cloud nonce %1, valid for another %2 sec")
        .arg(nonceCtx.nonce).arg((nonceCtx.expirationTime - curTime) / 1000));

    m_cdbNonceQueue.emplace_back(std::move(nonceCtx));
}

void CdbNonceFetcher::cloudBindingStatusChangedUnsafe(
    const QnMutexLockerBase& /*lock*/,
    bool boundToCloud)
{
    NX_DEBUG(this, lm("Cloud binding status changed: %1").arg(boundToCloud));

    if (!boundToCloud)
    {
        m_cdbNonceQueue.clear();
        return;
    }

    m_timer.post(
        [this]()
        {
            m_timer.cancelSync();
            fetchCdbNonceAsync();
        });
}

void CdbNonceFetcher::cloudBindingStatusChanged(bool boundToCloud)
{
    QnMutexLocker lock(&m_mutex);
    if (!boundToCloud)
        m_cloudUserInfoPool->clear(); //< TODO: #ak Remove it from here!
    cloudBindingStatusChangedUnsafe(lock, boundToCloud);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
