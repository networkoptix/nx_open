/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CDB_NONCE_FETCHER_H
#define NX_MS_CDB_NONCE_FETCHER_H

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <random>

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <cdb/connection.h>
#include <nx/utils/timer_manager.h>
#include <nx/network/http/httptypes.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/safe_direct_connection.h>

#include "abstract_nonce_provider.h"


class CloudConnectionManager;

/*!
    If server connected to cloud generates nonce suitable for authentication with cloud account credentials.
    Otherwise, standard nonce generation/validation logic is used
*/
class CdbNonceFetcher
:
    public AbstractNonceProvider,
    public QObject,
    public Qn::EnableSafeDirectConnection
{
public:
    /*!
        \param defaultGenerator Used if no connection to cloud
    */
    CdbNonceFetcher(
        CloudConnectionManager* const cloudConnectionManager,
        std::shared_ptr<AbstractNonceProvider> defaultGenerator);
    ~CdbNonceFetcher();

    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

    bool isValidCloudNonce(const QByteArray& nonce) const;

    static bool parseCloudNonce(
        const nx_http::BufferType& nonce,
        nx_http::BufferType* const cloudNonce,
        nx_http::BufferType* const nonceTrailer);

private:
    struct NonceCtx
    {
        QByteArray nonce;
        //!time we remove nonce from queue
        //TODO #ak #msvc2015 replace with std::chrono::time_point<std::chrono::steady_clock>
        qint64 validityTime;
        //!time, we stop report this nonce to caller
        qint64 expirationTime;
    };

    mutable QnMutex m_mutex;
    CloudConnectionManager* const m_cloudConnectionManager;
    std::shared_ptr<AbstractNonceProvider> m_defaultGenerator;
    bool m_boundToCloud;
    //map<cdb_nonce, valid_time>
    mutable std::deque<NonceCtx> m_cdbNonceQueue;
    QElapsedTimer m_monotonicClock;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;
    nx::utils::TimerManager::TimerGuard m_timerID;
    std::random_device m_rd;
    std::default_random_engine m_randomEngine;
    std::uniform_int_distribution<short> m_nonceTrailerRandomGenerator;

    void fetchCdbNonceAsync();
    void gotNonce(nx::cdb::api::ResultCode resCode, nx::cdb::api::NonceData nonce);

    static void removeInvalidNonce(
        std::deque<NonceCtx>* const cdbNonceQueue,
        qint64 curClock);

private slots:
    void cloudBindingStatusChanged(bool boundToCloud);
};

#endif  //NX_MS_CDB_NONCE_FETCHER_H
