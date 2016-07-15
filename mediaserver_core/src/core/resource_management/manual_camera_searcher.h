#ifndef MANUAL_CAMERA_SEARCHER_H
#define MANUAL_CAMERA_SEARCHER_H

#include <memory>

#include <QtCore/QFuture>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_searcher.h>
#include <utils/common/concurrent.h>
#include <utils/common/threadqueue.h>
#include <utils/network/ip_range_checker_async.h>


class QnSearchTask
{
    
public:
    typedef QList<QnAbstractNetworkResourceSearcher*> SearcherList;
    typedef std::function<void(
        const QnManualCameraSearchCameraList& results, QnSearchTask* const task)> SearchDoneCallback;

    QnSearchTask(){};

    QnSearchTask(
        const QString& addr, 
        int port, 
        const QAuthenticator& auth, 
        bool breakOnGotResult = false);

    void setSearchers(const SearcherList& searchers);
    void setSearchDoneCallback(const SearchDoneCallback& callback);
    void setBlocking(bool isBlocking);
    void setInterruptTaskProcessing(bool interrupt);

    bool isBlocking() const;
    bool doesInterruptTaskProcessing() const;

    void doSearch();



    QUrl url();

private:
    QUrl m_url;
    QAuthenticator m_auth;
    bool m_breakIfGotResult;
    bool m_blocking;
    bool m_interruptTaskProcesing;
    SearchDoneCallback m_callback;
    SearcherList m_searchers;
};


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
    QStringList getOnlineHosts(
        const QString& startAddr, 
        const QString& endAddr, 
        int port = nx_http::DEFAULT_HTTP_PORT);

    void runTasks(QThreadPool* threadPool, const QString& queueName);

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

    QnWaitCondition m_waitCondition;

    int m_totalTaskCount;
    QHash<QString, int> m_runningTaskCount;
    QHash<QString, QQueue<QnSearchTask>> m_urlSearchTaskQueues;
};

#endif // MANUAL_CAMERA_SEARCHER_H
