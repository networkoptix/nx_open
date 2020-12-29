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
class QnManualCameraSearcher: public /*mixin*/ QnCommonModuleAware, public nx::network::QnStoppableAsync
{
    using SearchDoneCallback = nx::utils::MoveOnlyFunc<void(QnManualCameraSearcher*)>;

public:
    QnManualCameraSearcher(QnCommonModule* commonModule);
    virtual ~QnManualCameraSearcher() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * Tries to find a camera on the host specified by the URL.
     * @param callback Will be called when the search is done.
     * @param url Must contain a host field, other URL fields are optional.
     */
    void startSearch(SearchDoneCallback callback, nx::utils::Url url);

    /**
     * Scans a range of IPv4 addresses and tries to find cameras on online hosts.
     * @param callback Will be called when the search is done.
     * @param startAddr The begining of the IP range to check. startAddr is checked too.
     * @param endAddr The end of the IP range to check. endAddr is checked too.
     * @param baseUrl The URL where the host will be substituted by the IP address of the found
     *     online hosts.
     */
    void startRangeSearch(SearchDoneCallback callback,
        nx::network::HostAddress startAddr,
        nx::network::HostAddress endAddr,
        nx::utils::Url baseUrl);

    QnManualCameraSearchProcessStatus status() const;

private:
    void startOnlineHostsScan(
        nx::network::HostAddress startAddr,
        nx::network::HostAddress endAddr,
        nx::utils::Url baseUrl);

    void onOnlineHostsScanDone(
        nx::utils::Url baseUrl, std::vector<nx::network::HostAddress> onlineHosts = {});
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
