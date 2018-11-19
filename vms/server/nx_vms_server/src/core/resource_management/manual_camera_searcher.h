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

// TODO: make sure threadPool is available on any call and it is not possible to destroy threadPool
//      before QnSearchTask;
// TODO: The way to cancel task should be added. Inherit from QnStoppable and implement pleaseStop.
class QnSearchTask
{
public:
    typedef QList<QnAbstractNetworkResourceSearcher*> SearcherList;
    typedef std::function<void(
        const QnManualResourceSearchList& results, QnSearchTask* const task)> SearchDoneCallback;

    QnSearchTask() = delete;
    QnSearchTask(
        QnCommonModule* commonModule,
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
    QnCommonModule * m_commonModule = nullptr;
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

//! Scans different addresses simultaneously (using aio or concurrent operations)
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

    using SearchDoneCallback = nx::utils::MoveOnlyFunc<void(QnManualCameraSearcher*)>;

public:
    QnManualCameraSearcher(QnCommonModule* commonModule);
    ~QnManualCameraSearcher();

    bool run(SearchDoneCallback callback, const QString& startAddr, const QString& endAddr,
        const QAuthenticator& auth, int port);
    void cancel();
    QnManualCameraSearchProcessStatus status() const;

private:
    QStringList getOnlineHosts(
        const QString& startAddr,
        const QString& endAddr,
        int port = nx::network::http::DEFAULT_HTTP_PORT);

    // TODO: Rename?
    void searchTaskDoneHandler(const QnManualResourceSearchList& results, QnSearchTask* const task);

    void changeStateUnsafe(QnManualResourceSearchStatus::State newState);
    void runTasksUnsafe();
    QList<QnAbstractNetworkResourceSearcher*> getAllNetworkSearchers() const;

private:
    mutable QnMutex m_mutex;
    QnManualResourceSearchList m_results;
    QnManualResourceSearchStatus::State m_state;
    std::atomic<bool> m_cancelled;

    QnIpRangeCheckerAsync m_ipChecker;
    int m_hostRangeSize;

    SearchDoneCallback m_searchDoneCallback;

    int m_totalTaskCount;
    int m_remainingTaskCount;

    QMap<QString, QQueue<QnSearchTask>> m_urlSearchTaskQueues;
    QMap<QString, SearchTaskQueueContext> m_searchQueueContexts;
};

#endif // MANUAL_CAMERA_SEARCHER_H
