#ifndef MANUAL_CAMERA_SEARCHER_H
#define MANUAL_CAMERA_SEARCHER_H

#include <memory>
#include <atomic>

#include <QtCore/QFuture>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_searcher.h>
#include <nx/utils/concurrent.h>
#include <utils/common/threadqueue.h>
#include <nx/network/ip_range_checker_async.h>
#include <common/common_module_aware.h>


class QnSearchTask
{

public:
    typedef QList<QnAbstractNetworkResourceSearcher*> SearcherList;
    typedef std::function<void(
        const QnManualResourceSearchList& results, QnSearchTask* const task)> SearchDoneCallback;

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

    nx::utils::Url url();
    QString toString();

private:
    nx::utils::Url m_url;
    QAuthenticator m_auth;

    /**
     * If one of the searchers in task found a resource than other searchers in the same task
     * won't be launched.
     */
    bool m_breakIfGotResult;

    // Need to wait while task will be done before launching next task in queue.
    bool m_blocking;

    // If we got some results from this task then other tasks in queue shouldn't be launched.
    bool m_interruptTaskProcesing;
    SearchDoneCallback m_callback;
    SearcherList m_searchers;
};


//!Scans different addresses simultaneously (using aio or concurrent operations)
class QnManualCameraSearcher: public QnCommonModuleAware
{

    struct SearchTaskQueueContext
    {
        SearchTaskQueueContext() :
            isBlocked(false),
            isInterrupted(false),
            runningTaskCount(0) {};

        bool isBlocked;
        bool isInterrupted;
        int runningTaskCount;
    };

public:
    QnManualCameraSearcher(QnCommonModule* commonModule);
    ~QnManualCameraSearcher();

    /*!
        \param pool Thread pool to use for running concurrent operations
    */
    bool run( QThreadPool* pool, const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port );
    void cancel();
    QnManualCameraSearchProcessStatus status() const;

private:
    QStringList getOnlineHosts(
        const QString& startAddr,
        const QString& endAddr,
        int port = nx::network::http::DEFAULT_HTTP_PORT);

    void runTasksUnsafe(QThreadPool* threadPool);
    QList<QnAbstractNetworkResourceSearcher*> getAllNetworkSearchers() const;
private:
    mutable QnMutex m_mutex;

    QnManualResourceSearchList m_results;
    QnManualResourceSearchStatus::State m_state;
    std::atomic<bool> m_cancelled;

    QnIpRangeCheckerAsync m_ipChecker;
    int m_hostRangeSize;

    QnWaitCondition m_waitCondition;

    int m_totalTaskCount;
    int m_remainingTaskCount;

    QMap<QString, QQueue<QnSearchTask>> m_urlSearchTaskQueues;
    QMap<QString, SearchTaskQueueContext> m_searchQueueContexts;

};

#endif // MANUAL_CAMERA_SEARCHER_H
