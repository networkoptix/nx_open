#include "manual_camera_searcher.h"

#include <type_traits>
#include <functional>

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_searcher.h>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <plugins/resource/archive_camera/archive_camera.h>
#include <common/common_module.h>

static const int MAX_PERCENT = 100;
//assuming that scanning ports is ten times faster than plugin check
static const int PORT_SCAN_MAX_PROGRESS_PERCENT = 10;
static_assert( PORT_SCAN_MAX_PROGRESS_PERCENT < MAX_PERCENT, "PORT_SCAN_MAX_PROGRESS_PERCENT must be less than MAX_PERCENT" );


QnManualCameraSearcher::QnManualCameraSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_state(QnManualResourceSearchStatus::Init),
    m_hostRangeSize(0),
    m_ipScanner(m_pollable.getAioThread()),
    m_taskManager(commonModule, m_pollable.getAioThread())
{
    NX_VERBOSE(this, "Created");
}

QnManualCameraSearcher::~QnManualCameraSearcher()
{
    NX_VERBOSE(this, "Destroying (state: %1)", m_state.load());
    pleaseStopSync();
}

void QnManualCameraSearcher::pleaseStop(nx::utils::MoveOnlyFunc<void ()> completionHandler)
{
    // Stopping: ip scanner -> task manager -> m_pollable -> completionHandler called.

    auto onTaskManagerStop =
        [this, handler = std::move(completionHandler)]() mutable
        {
            if (m_state != QnManualResourceSearchStatus::Finished)
                changeState(QnManualResourceSearchStatus::Aborted);
            m_pollable.pleaseStop(std::move(handler));
        };

    auto onIpCheckerStop =
        [this, handler = std::move(onTaskManagerStop)]() mutable
        {
            m_taskManager.pleaseStop(std::move(handler));
        };

    NX_VERBOSE(this, "Canceling search");
    m_ipScanner.pleaseStop(std::move(onIpCheckerStop));
}

QList<QnAbstractNetworkResourceSearcher*> QnManualCameraSearcher::getAllNetworkSearchers() const
{
    QList<QnAbstractNetworkResourceSearcher*> result;

    for (QnAbstractResourceSearcher* as : commonModule()->resourceDiscoveryManager()->plugins())
    {
        QnAbstractNetworkResourceSearcher* ns =
            dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
        NX_CRITICAL(ns);

        result.push_back(ns);
    }

    return result;
}

void QnManualCameraSearcher::startSearch(SearchDoneCallback callback, nx::utils::Url url)
{
    NX_ASSERT(url.isValid());

    m_pollable.dispatch(
        [this, callback = std::move(callback), url = std::move(url)]() mutable
        {
            NX_ASSERT(m_state == QnManualResourceSearchStatus::Init);
            m_searchDoneCallback = std::move(callback);
            onOnlineHostsScanDone(url, {nx::network::HostAddress(url.host())});
        });
}

void QnManualCameraSearcher::startRangeSearch(
    SearchDoneCallback callback,
    nx::network::HostAddress startAddr,
    nx::network::HostAddress endAddr,
    nx::utils::Url baseUrl)
{
    NX_ASSERT(startAddr.isIpAddress());
    NX_ASSERT(endAddr.isIpAddress());

    m_pollable.dispatch(
        [this, callback = std::move(callback), startAddr, endAddr, url = std::move(baseUrl)]() mutable
        {
            NX_ASSERT(m_state == QnManualResourceSearchStatus::Init);
            m_searchDoneCallback = std::move(callback);
            startOnlineHostsScan(std::move(startAddr), std::move(endAddr), std::move(url));
        });
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const
{
    return m_pollable.executeInAioThreadSync(
        [this]() -> QnManualCameraSearchProcessStatus
        {
            QnManualCameraSearchProcessStatus result;
            switch (m_state)
            {
                case QnManualResourceSearchStatus::Finished:
                case QnManualResourceSearchStatus::Aborted:
                {
                    result.cameras = m_results;
                    result.status = QnManualResourceSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
                    break;
                }
                case QnManualResourceSearchStatus::CheckingOnline:
                {
                    NX_ASSERT(m_hostRangeSize);
                    int currentProgress = m_hostRangeSize
                        ? (int) m_ipScanner.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT
                            / m_hostRangeSize
                        : 0;

                    result.status = QnManualResourceSearchStatus(m_state, currentProgress, MAX_PERCENT);
                    break;
                }
                case QnManualResourceSearchStatus::CheckingHost:
                {
                    result.cameras = m_taskManager.foundResources();
                    auto currentPercent = std::floor(0.5
                        + (static_cast<double>(MAX_PERCENT) - PORT_SCAN_MAX_PROGRESS_PERCENT)
                        * m_taskManager.doneToTotalTasksRatio()) + PORT_SCAN_MAX_PROGRESS_PERCENT;
                    result.status = QnManualResourceSearchStatus(m_state, currentPercent, MAX_PERCENT);
                    break;
                }
                default:
                {
                    result.status = QnManualResourceSearchStatus(m_state, 0, MAX_PERCENT);
                    break;
                }
            }

            NX_DEBUG(this, "Status: %1 (%2 max)", result.status.current, result.status.total);
            return result;
        });
}

void QnManualCameraSearcher::startOnlineHostsScan(
    nx::network::HostAddress startAddr,
    nx::network::HostAddress endAddr,
    nx::utils::Url baseUrl)
{
    NX_VERBOSE(this, "Getting online hosts in range [%1, %2]", startAddr, endAddr);
    const auto oldState = changeState(QnManualResourceSearchStatus::CheckingOnline);
    NX_ASSERT(oldState == QnManualResourceSearchStatus::Init);

    m_hostRangeSize = ntohl(endAddr.ipV4().get().s_addr) - ntohl(startAddr.ipV4().get().s_addr) + 1;
    NX_ASSERT(m_hostRangeSize > 0);

    int port = baseUrl.port(nx::network::http::DEFAULT_HTTP_PORT);
    m_ipScanner.scanOnlineHosts(
        [this, baseUrl = std::move(baseUrl)](auto results)
        {
            onOnlineHostsScanDone(baseUrl, std::move(results));
        },
        std::move(startAddr), std::move(endAddr), port);
}

void QnManualCameraSearcher::onOnlineHostsScanDone(
    nx::utils::Url baseUrl, std::vector<nx::network::HostAddress> onlineHosts)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(m_state == QnManualResourceSearchStatus::Init
        || m_state == QnManualResourceSearchStatus::CheckingOnline);
    NX_VERBOSE(this, "Will check %1 hosts", onlineHosts.size());

    QnSearchTask::SearcherList sequentialSearchers;
    QnSearchTask::SearcherList parallelSearchers;
    auto searchers = getAllNetworkSearchers();
    for (const auto& searcher: searchers)
    {
        if (searcher->isSequential())
            sequentialSearchers.push_back(searcher);
        else
            parallelSearchers.push_back(searcher);
    }
    NX_VERBOSE(this, "Will use %1 sequential and %2 concurrent searchers",
        sequentialSearchers.size(), parallelSearchers.size());

    for (const auto& host: onlineHosts)
    {
        const auto url = nx::network::url::Builder(baseUrl).setHost(host.toString()).toUrl();
        m_taskManager.addTask(url, sequentialSearchers, /*isSequential*/ true);

        for (const auto& searcher: parallelSearchers)
            m_taskManager.addTask(url, {searcher}, /*isSequential*/ false);
    }

    changeState(QnManualResourceSearchStatus::CheckingHost);
    m_taskManager.startTasks([this](auto result){ onManualSearchDone(std::move(result)); });
}

void QnManualCameraSearcher::onManualSearchDone(
    QnManualResourceSearchList results)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(m_state == QnManualResourceSearchStatus::CheckingHost);
    changeState(QnManualResourceSearchStatus::Finished);

    m_results = std::move(results);
    NX_ASSERT(m_searchDoneCallback);
    m_searchDoneCallback(this);
}

QnManualResourceSearchStatus::State QnManualCameraSearcher::changeState(
    QnManualResourceSearchStatus::State newState)
{
    auto oldState = m_state.exchange(newState);
    NX_VERBOSE(this, "State change: %1 -> %2", oldState, newState);
    return oldState;
}
