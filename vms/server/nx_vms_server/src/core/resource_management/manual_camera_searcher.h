#ifndef MANUAL_CAMERA_SEARCHER_H
#define MANUAL_CAMERA_SEARCHER_H

#include <memory>
#include <atomic>

#include <QtCore/QFuture>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/concurrent.h>
#include <utils/common/threadqueue.h>
#include <nx/network/ip_range_checker_async.h>
#include <common/common_module_aware.h>

#include "manual_camera_search_task_manager.h"

//! Scans different addresses simultaneously (using aio or concurrent operations)
class QnManualCameraSearcher: public QnCommonModuleAware
{
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
    void searchTaskDoneHandler(QnManualResourceSearchList results);

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
    QnManualSearchTaskManager m_taskManager;

    SearchDoneCallback m_searchDoneCallback;
};

#endif // MANUAL_CAMERA_SEARCHER_H
