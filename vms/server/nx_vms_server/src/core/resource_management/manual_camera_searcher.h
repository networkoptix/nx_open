#pragma once

#include <memory>
#include <atomic>

#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/ip_range_scanner.h>
#include <common/common_module_aware.h>

#include "manual_camera_search_task_manager.h"

//! Scans different addresses simultaneously (using aio or concurrent operations).
class QnManualCameraSearcher: public QnCommonModuleAware, public nx::network::QnStoppableAsync
{
    using SearchDoneCallback = nx::utils::MoveOnlyFunc<void(QnManualCameraSearcher*)>;

public:
    QnManualCameraSearcher(QnCommonModule* commonModule);
    virtual ~QnManualCameraSearcher() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    void run(SearchDoneCallback callback,
        nx::network::HostAddress startAddr,
        std::optional<nx::network::HostAddress> endAddr,
        QAuthenticator auth,
        int port);

    QnManualCameraSearchProcessStatus status() const;

private:
    void startOnlineHostsScan(
        nx::network::HostAddress startAddr,
        nx::network::HostAddress endAddr,
        QAuthenticator auth,
        int port = nx::network::http::DEFAULT_HTTP_PORT);

    void onOnlineHostsScanDone(
        std::vector<nx::network::HostAddress> onlineHosts,
        int port,
        QAuthenticator auth);
    void onManualSearchDone(QnManualResourceSearchList results);

    QnManualResourceSearchStatus::State changeState(QnManualResourceSearchStatus::State newState);
    void runTasks();
    QList<QnAbstractNetworkResourceSearcher*> getAllNetworkSearchers() const;

private:
    QnManualResourceSearchList m_results;
    std::atomic<QnManualResourceSearchStatus::State> m_state;

    mutable nx::network::aio::BasicPollable m_pollable;
    int m_hostRangeSize;
    nx::network::IpRangeScanner m_ipScanner;
    QnManualSearchTaskManager m_taskManager;

    SearchDoneCallback m_searchDoneCallback;
};
