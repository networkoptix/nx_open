#ifndef MANUAL_CAMERA_SEARCHER_H
#define MANUAL_CAMERA_SEARCHER_H

#include <QtCore/QFuture>

#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>

#include <core/resource/resource_fwd.h>

class QnManualCameraSearcher {
public:
    QnManualCameraSearcher();
    ~QnManualCameraSearcher();

    void run(const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port);
    void cancel();

    QnManualCameraSearchProcessStatus status() const;
protected:

    bool isCancelled() const;
private:
    /** Mutex that is used to synchronize access to private fields. */
    mutable QMutex m_mutex;

    QnManualCameraSearchStatus::State m_state;
    bool m_singleAddressCheck;
    QFuture<QStringList> *m_onlineHosts;
    QFuture<QnManualCameraSearchCameraList> *m_scanProgress;
    QnManualCameraSearchCameraList m_results;
};

#endif // MANUAL_CAMERA_SEARCHER_H
