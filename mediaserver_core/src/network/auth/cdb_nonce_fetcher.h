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

#include <boost/optional.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <cdb/connection.h>
#include <utils/common/timermanager.h>
#include <utils/thread/mutex.h>

#include "abstract_nonce_provider.h"


/*!
    If server connected to cloud generates nonce suitable for authentication with cloud account credentials.
    Otherwise, standard nonce generation/validation logic is used
*/
class CdbNonceFetcher
:
    public AbstractNonceProvider,
    public QObject
{
public:
    /*!
        \param defaultGenerator Used if no connection to cloud
    */
    CdbNonceFetcher(std::unique_ptr<AbstractNonceProvider> defaultGenerator);
    ~CdbNonceFetcher();

    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

private:
    struct NonceCtx
    {
        QByteArray nonce;
        //!time we remove nonce from queue
        //TODO #ak #msvc2015 replace with std::chrono::time_point<std::chrono::steady_clock>
        qint64 expirationTime;
        //!time, we stop report this nonce to caller
        qint64 validityTime;
    };

    mutable QnMutex m_mutex;
    std::unique_ptr<AbstractNonceProvider> m_defaultGenerator;
    std::atomic<bool> m_bindedToCloud;
    //map<cdb_nonce, valid_time>
    std::deque<NonceCtx> m_cdbNonceQueue;
    QElapsedTimer m_monotonicClock;
    std::unique_ptr<nx::cdb::api::Connection> m_connection;
    TimerManager::TimerGuard m_timerID;

    void fetchCdbNonceAsync();
    void gotNonce(nx::cdb::api::ResultCode resCode, nx::cdb::api::NonceData nonce);
    void removeExpiredNonce(QnMutexLockerBase* const lk);

private slots:
    void cloudBindingStatusChanged(bool bindedToCloud);
};

#endif  //NX_MS_CDB_NONCE_FETCHER_H
