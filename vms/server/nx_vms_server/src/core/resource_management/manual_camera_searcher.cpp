
#include "manual_camera_searcher.h"

#include <type_traits>
#include <functional>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentMap>

#include <utils/common/scoped_thread_rollback.h>
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


// TODO: Add UUID here.
QnManualCameraSearcher::QnManualCameraSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_state(QnManualResourceSearchStatus::Init),
    m_hostRangeSize(0),
    m_ipChecker(m_pollable.getAioThread()),
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
    m_pollable.dispatch(
        [this, handler = std::move(completionHandler)]() mutable
        {
            NX_VERBOSE(this, "Canceling search");
            m_pollable.pleaseStopSync();
            // TODO: Is it possible that after pleaseStopSync posts occur? How does BasicPollable
            // handle posts after pleaseStop?

            switch (m_state) {
            case QnManualResourceSearchStatus::State::CheckingOnline:
                m_ipChecker.pleaseStop(std::move(handler));
                break;
            case QnManualResourceSearchStatus::State::CheckingHost:
                m_taskManager.pleaseStop(std::move(handler));
                break;
            default:
                handler();
                break;
            }
            changeState(QnManualResourceSearchStatus::Aborted);
        });
}

QList<QnAbstractNetworkResourceSearcher*> QnManualCameraSearcher::getAllNetworkSearchers() const
{
    QList<QnAbstractNetworkResourceSearcher*> result;

    for (QnAbstractResourceSearcher* as : commonModule()->resourceDiscoveryManager()->plugins())
    {
        QnAbstractNetworkResourceSearcher* ns =
            dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
        NX_ASSERT(ns);

        result.push_back(ns);
    }

    return result;
}

void QnManualCameraSearcher::run(
    SearchDoneCallback callback,
    const QString& startAddr,
    const QString& endAddr,
    const QAuthenticator& auth,
    int port)
{
    m_pollable.dispatch(
        [=, callback = std::move(callback)]() mutable
        {
            NX_ASSERT(m_state == QnManualResourceSearchStatus::Init);
            m_searchDoneCallback = std::move(callback);

            if (endAddr.isNull())
                onOnlineHostsScanDone({std::move(startAddr)}, port, std::move(auth));
            else
                startOnlineHostsScan(std::move(startAddr), std::move(endAddr), std::move(auth), port);

            return;
        });
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const
{
    QnManualCameraSearchProcessStatus result;
    std::promise<void> resultPromise;
    auto resultReady = resultPromise.get_future();

    m_pollable.dispatch(
        [this, &result, &resultPromise]()
        {
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
                        ? (int) m_ipChecker.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT
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
            resultPromise.set_value();
        });

    resultReady.wait();
    return result;
}

void QnManualCameraSearcher::startOnlineHostsScan(
    QString startAddr,
    QString endAddr,
    QAuthenticator auth,
    int port)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(m_state == QnManualResourceSearchStatus::Init);
    NX_VERBOSE(this, "Getting online hosts in range (%1, %2)", startAddr, endAddr);

    const quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
    const quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();

    NX_ASSERT(endIPv4Addr >= startIPv4Addr); // TODO: implement that check in request

    m_hostRangeSize = endIPv4Addr - startIPv4Addr;
    changeState(QnManualResourceSearchStatus::CheckingOnline);

    m_ipChecker.onlineHosts(
        [this, port, auth = std::move(auth)](auto results){ onOnlineHostsScanDone(results, port, auth); },
        QHostAddress(startAddr),
        QHostAddress(endAddr),
        port);
}

void QnManualCameraSearcher::onOnlineHostsScanDone(
    QStringList onlineHosts, int port, QAuthenticator auth)
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
        m_taskManager.addTask(nx::network::SocketAddress(host, port),
            auth, sequentialSearchers, /*isSequential*/ true);

        for (const auto& searcher: parallelSearchers)
        {
            m_taskManager.addTask(nx::network::SocketAddress(host, port),
                auth, {searcher}, /*isSequential*/ false);
        }
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

void QnManualCameraSearcher::changeState(QnManualResourceSearchStatus::State newState)
{
    NX_VERBOSE(this, "State change: %1 -> %2", m_state.load(), newState);
    m_state = newState;
}

