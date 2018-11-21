
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
    m_cancelled(false),
    m_hostRangeSize(0),
    m_taskManager(commonModule)
{
    NX_VERBOSE(this, "Created");
}

QnManualCameraSearcher::~QnManualCameraSearcher()
{
    NX_VERBOSE(this, "Destroying (state: %1)", m_state);
    cancel();
    // TODO: should make sure that all tasks has finished and notify cond var!
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

// TODO: Use QnIpRangeCheckerAsync asynchronously.
bool QnManualCameraSearcher::run(
    SearchDoneCallback callback,
    const QString& startAddr,
    const QString& endAddr,
    const QAuthenticator& auth,
    int port)
{
    {
        QnMutexLocker lock(&m_mutex);
        // TODO: Use assert? Allow to run only from init state?
        if (m_state == QnManualResourceSearchStatus::Aborted)
            return false;

        m_cancelled = false;
        m_searchDoneCallback = std::move(callback);
        // TODO: reset TaskManager
    }

    QStringList onlineHosts;
    if (endAddr.isNull())
        onlineHosts.push_back(startAddr);
    else
        onlineHosts = getOnlineHosts(startAddr, endAddr, port);
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

    {
        QnMutexLocker lock(&m_mutex);
        // TODO: Should I run callback here? It seems yes...
        if (m_cancelled)
            return true;
        changeStateUnsafe(QnManualResourceSearchStatus::CheckingHost);
        m_taskManager.startTasks([this](auto result){ searchTaskDoneHandler(std::move(result)); });
    }

    return true;
}

// TODO: revise it implementation and usage.
// Implement pleaseStop pair?
void QnManualCameraSearcher::cancel()
{
    {
        QnMutexLocker lock(&m_mutex);
        NX_VERBOSE(this, "Canceling search");
        m_cancelled = true;
        switch (m_state)
        {
            case QnManualResourceSearchStatus::Finished:
                return;
            case QnManualResourceSearchStatus::CheckingOnline:
                m_ipChecker.pleaseStop();
                m_ipChecker.join(); // TODO: remove it from here
                break;
            case QnManualResourceSearchStatus::CheckingHost:
                m_taskManager.pleaseStopSync();  // TODO: make it async
                break;
            default:
                break;
        }
        changeStateUnsafe(QnManualResourceSearchStatus::Aborted);
    }
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const
{
    QnMutexLocker lock(&m_mutex);
    QnManualCameraSearchProcessStatus result;

    switch (m_state)
    {
        case QnManualResourceSearchStatus::CheckingHost:
        {
            // TODO: Should take it from m_taskManager if required such logic!
//            result.cameras = m_results;
            break;
        }
        case QnManualResourceSearchStatus::Finished:
        case QnManualResourceSearchStatus::Aborted:
        {
            result.cameras = m_results;
            break;
        }
        default:
        {
            break;
        }
    }

    switch (m_state)
    {
        case QnManualResourceSearchStatus::Finished:
        case QnManualResourceSearchStatus::Aborted:
        {
            result.status = QnManualResourceSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
            NX_DEBUG(this, "Status: %1 (%2 max)", result.status.current, result.status.total);
            break;
        }
        case QnManualResourceSearchStatus::CheckingOnline:
        {
            Q_ASSERT(m_hostRangeSize);
            int currentProgress = m_hostRangeSize
                ? (int) m_ipChecker.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT
                    / m_hostRangeSize
                : 0;

            result.status = QnManualResourceSearchStatus(m_state, currentProgress, MAX_PERCENT);
            NX_DEBUG(this, "Status: %1 (%2 max)", result.status.current, result.status.total);
            break;
        }
        case QnManualResourceSearchStatus::CheckingHost:
        {
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

    return result;
}

QStringList QnManualCameraSearcher::getOnlineHosts(
    const QString& startAddr,
    const QString& endAddr,
    int port)
{
    NX_VERBOSE(this, "Getting online hosts in range (%1, %2)", startAddr, endAddr);
    QStringList onlineHosts;

    const quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
    const quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();

    if (endIPv4Addr < startIPv4Addr)
        return QStringList();

    {
        QnMutexLocker lock(&m_mutex);
        m_hostRangeSize = endIPv4Addr - startIPv4Addr;
        if (m_cancelled)
            return QStringList();
        changeStateUnsafe(QnManualResourceSearchStatus::CheckingOnline);
    }

    onlineHosts = m_ipChecker.onlineHosts(
        QHostAddress(startAddr),
        QHostAddress(endAddr),
        port ? port : nx::network::http::DEFAULT_HTTP_PORT );

    {
        QnMutexLocker lock( &m_mutex );
        if (m_cancelled)
            return QStringList();
    }

    return onlineHosts;
}

void QnManualCameraSearcher::searchTaskDoneHandler(
    QnManualResourceSearchList results)
{
    QnMutexLocker lock(&m_mutex);
    m_results = std::move(results);
    if (!m_cancelled)
        changeStateUnsafe(QnManualResourceSearchStatus::Finished);
    else
        NX_ASSERT(m_state == QnManualResourceSearchStatus::Aborted);

    NX_ASSERT(m_searchDoneCallback);
    m_searchDoneCallback(this);
}

void QnManualCameraSearcher::changeStateUnsafe(QnManualResourceSearchStatus::State newState)
{
    NX_VERBOSE(this, "State change: %1 -> %2", m_state, newState);
    m_state = newState;
}

