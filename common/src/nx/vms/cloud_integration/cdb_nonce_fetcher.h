#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <random>

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <nx/network/http/http_types.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/safe_direct_connection.h>

#include <nx/cloud/cdb/api/connection.h>
#include <nx/vms/auth/abstract_nonce_provider.h>

#include "cloud_user_info_pool.h"

namespace nx {
namespace vms {
namespace cloud_integration {

class AbstractCloudConnectionManager;
class AbstractCloudUserInfoPool;

/**
 * If server connected to cloud generates nonce suitable for authentication with cloud account credentials.
 * Otherwise, standard nonce generation/validation logic is used.
 */
class CdbNonceFetcher:
    public auth::AbstractNonceProvider,
    public QObject,
    public Qn::EnableSafeDirectConnection
{
public:
    /**
     * @param defaultGenerator Used if no connection to cloud.
     */
    CdbNonceFetcher(
        nx::utils::StandaloneTimerManager* timerManager,
        AbstractCloudConnectionManager* const cloudConnectionManager,
        AbstractCloudUserInfoPool* cloudUserInfoPool,
        auth::AbstractNonceProvider* defaultGenerator);
    ~CdbNonceFetcher();

    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

    bool isValidCloudNonce(const QByteArray& nonce) const;
    nx::cdb::api::ResultCode initializeConnectionToCloudSync();

    static bool parseCloudNonce(
        const nx_http::BufferType& nonce,
        nx_http::BufferType* const cloudNonce,
        nx_http::BufferType* const nonceTrailer);

protected:
    static nx::Buffer generateNonceTrailer(std::function<short()> genFunc);

private:
    struct NonceCtx
    {
        QByteArray nonce;
        /** Time we remove nonce from queue. */
        //TODO #ak #msvc2015 replace with std::chrono::time_point<std::chrono::steady_clock>
        qint64 validityTime;
        /** Time, we stop report this nonce to caller. */
        qint64 expirationTime;
    };

    mutable QnMutex m_mutex;
    AbstractCloudConnectionManager* const m_cloudConnectionManager;
    auth::AbstractNonceProvider* m_defaultGenerator;
    /** map<cdb_nonce, valid_time>. */
    mutable std::deque<NonceCtx> m_cdbNonceQueue;
    QElapsedTimer m_monotonicClock;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;
    nx::utils::TimerManager::TimerGuard m_timerID;
    std::random_device m_rd;
    std::default_random_engine m_randomEngine;
    std::uniform_int_distribution<short> m_nonceTrailerRandomGenerator;
    nx::utils::StandaloneTimerManager* m_timerManager;
    AbstractCloudUserInfoPool* m_cloudUserInfoPool;

    void fetchCdbNonceAsync();
    void gotNonce(nx::cdb::api::ResultCode resCode, nx::cdb::api::NonceData nonce);
    void saveCloudNonce(nx::cdb::api::NonceData nonce);
    void removeExpiredNonce(const QnMutexLockerBase&, qint64 curClock);

    void cloudBindingStatusChangedUnsafe(const QnMutexLockerBase&, bool boundToCloud);
    void cloudBindingStatusChanged(bool boundToCloud);
    nx::Buffer generateNonceTrailer();
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
