#ifndef MANUAL_CAMERA_SEARCHER_H
#define MANUAL_CAMERA_SEARCHER_H

#include <memory>

#include <QtCore/QFuture>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/concurrent.h>
#include <utils/network/ip_range_checker_async.h>


//!Scans different addresses simultaneously (using aio or concurrent operations)
class QnManualCameraSearcher {
public:
    QnManualCameraSearcher();
    ~QnManualCameraSearcher();

    /*!
        \param pool Thread pool to use for running concurrent operations
    */
    bool run( QThreadPool* pool, const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port );
    void cancel();

    QnManualCameraSearchProcessStatus status() const;

protected:
    bool isCancelled() const;

private:
    /** Mutex that is used to synchronize access to private fields. */
    mutable QnMutex m_mutex;

    QnManualCameraSearchStatus::State m_state;
    bool m_singleAddressCheck;
    QnConcurrent::QnFuture<QnManualCameraSearchCameraList>* m_scanProgress;
    QnManualCameraSearchCameraList m_results;
    bool m_cancelled;
    QnIpRangeCheckerAsync m_ipChecker;
    int m_hostRangeSize;
};

#endif // MANUAL_CAMERA_SEARCHER_H
