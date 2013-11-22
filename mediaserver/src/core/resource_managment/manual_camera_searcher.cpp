#include "manual_camera_searcher.h"

#include <QtConcurrent/QtConcurrentMap>

#include <core/resource_managment/resource_pool.h>
#include <core/resource_managment/resource_discovery_manager.h>
#include <core/resource_managment/resource_searcher.h>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/network/ip_range_checker.h>

namespace {
    /** Thread limit to scan for online hosts. */
    static const quint32 onlineScanThreadCount = 32;

    /**
     * @brief resourceExistsInPool          Check if found camera is already exists in pool by its unique ID. For onvif cameras host is also checked.
     * @param resource                      Camera resource.
     * @return                              True if resource is already in resource pool, false otherwise.
     */
    bool resourceExistsInPool(const QnResourcePtr &resource) {
        if (qnResPool->hasSuchResource(resource->getUniqueId()))
            return true; // already in resource pool

        // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition check for IP address
        if (QnNetworkResourcePtr netRes = resource.dynamicCast<QnNetworkResource>()) {
            QnNetworkResourceList existResList = qnResPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
            foreach(QnNetworkResourcePtr existRes, existResList) {
                if (existRes->getTypeId() != netRes->getTypeId())
                    return true; // camera found by different drivers

                QnVirtualCameraResourcePtr existCam = existRes.dynamicCast<QnVirtualCameraResource>();
                if (!existCam->isManuallyAdded())
                    return true; // block manual and auto add in same time
            }
        }
        return false;
    }

    QnManualCameraSearchSingleCamera fromResource(const QnResourcePtr &resource) {
        return QnManualCameraSearchSingleCamera(resource->getName(),
                                                resource->getUrl(),
                                                qnResTypePool->getResourceType(resource->getTypeId())->getName(),
                                                resourceExistsInPool(resource));
    }
}

struct SinglePluginChecker {

    SinglePluginChecker(QnAbstractNetworkResourceSearcher* plugin, const QUrl &url, const QAuthenticator &auth):
        plugin(plugin), url(url), auth(auth) {}


    QnManualCameraSearchCameraList mapFunction() const {
        QnManualCameraSearchCameraList results;
        foreach(const QnResourcePtr &resource, plugin->checkHostAddr(url, auth, true))
            results << fromResource(resource);
        if (url.toString().endsWith(QLatin1Char('1')))
            results << QnManualCameraSearchSingleCamera(QLatin1String("test"),
                                                        url.toString(),
                                                        QLatin1String("oololo"),
                                                        false);
        return results;
    }

    QnAbstractNetworkResourceSearcher* plugin;
    QUrl url;
    QAuthenticator auth;
};

struct PluginsEnumerator {
    PluginsEnumerator(const QString &addr, int port, const QAuthenticator &auth):
        auth(auth)
    {
        if( QUrl(addr).scheme().isEmpty() )
            url.setHost(addr);
        else
            url.setUrl(addr);
        if (port)
            url.setPort(port);
    }

    QList<SinglePluginChecker> enumerate() const {
        QList<SinglePluginChecker> result;
        foreach(QnAbstractResourceSearcher* as, QnResourceDiscoveryManager::instance()->plugins()) {
            QnAbstractNetworkResourceSearcher* ns = dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
            Q_ASSERT( ns );
            result << SinglePluginChecker(ns, url, auth);
        }
        return result;
    }

    static int pluginsCount() {
        return QnResourceDiscoveryManager::instance()->plugins().size();
    }

    QUrl url;
    QAuthenticator auth;
};

QnManualCameraSearcher::QnManualCameraSearcher():
    m_state(QnManualCameraSearchStatus::Init),
    m_onlineHosts(NULL),
    m_scanProgress(NULL)
{
}

QnManualCameraSearcher::~QnManualCameraSearcher() {
}

void QnManualCameraSearcher::run(const QString &startAddr, const QString &endAddr, const QAuthenticator &auth, int port) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == QnManualCameraSearchStatus::Aborted)
            return;
    }

    QList<SinglePluginChecker> checkers;

    bool singleAddressCheck = endAddr.isNull();
    if (singleAddressCheck) {
        checkers << PluginsEnumerator(startAddr, port, auth).enumerate();
    } else { //subnet scan
        quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
        quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();
        if (endIPv4Addr < startIPv4Addr)
            return;

        QStringList onlineHosts;
        QFuture<QStringList> future;
        {
            QN_INCREASE_MAX_THREADS(qMin(endIPv4Addr - startIPv4Addr, onlineScanThreadCount));
            {
                QMutexLocker lock(&m_mutex);
                if (m_state == QnManualCameraSearchStatus::Aborted)
                    return;
                m_state = QnManualCameraSearchStatus::CheckingOnline;
                future = QnIpRangeChecker::onlineHostsAsync(QHostAddress(startAddr), QHostAddress(endAddr), port ? port : 80);
                m_onlineHosts = &future;
            }
            future.waitForFinished();
            if (future.isCanceled())
                return;
            onlineHosts = future.result();
            {
                QMutexLocker lock(&m_mutex);
                m_onlineHosts = NULL;
                if (m_state == QnManualCameraSearchStatus::Aborted)
                    return;
                m_state = QnManualCameraSearchStatus::CheckingHost;
            }

        }

        foreach(const QString& addr, onlineHosts)
            checkers << PluginsEnumerator(addr, port, auth).enumerate();
    }

    {
        QFuture<QnManualCameraSearchCameraList> results;
        QN_INCREASE_MAX_THREADS(onlineScanThreadCount);
        {
            QMutexLocker lock(&m_mutex);
            if (m_state == QnManualCameraSearchStatus::Aborted)
                return;
            results = QtConcurrent::mapped(checkers, &SinglePluginChecker::mapFunction);
            m_scanProgress = &results;
        }
        results.waitForFinished();
        if (results.isCanceled())
            return;
        {
             QMutexLocker lock(&m_mutex);
             for (int i = 0; i < results.resultCount(); ++i)
                 m_results << results.resultAt(i);
             m_scanProgress = NULL;
             if (m_state == QnManualCameraSearchStatus::Aborted)
                 return;
             m_state = QnManualCameraSearchStatus::Finished;
        }
    }

}

void QnManualCameraSearcher::cancel() {
    QMutexLocker lock(&m_mutex);
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
        return;
    case QnManualCameraSearchStatus::CheckingOnline:
        if (!m_onlineHosts)
            return;
        m_onlineHosts->cancel();
        m_onlineHosts->waitForFinished();
        break;
    case QnManualCameraSearchStatus::CheckingHost:
        if (!m_scanProgress)
            return;
        m_scanProgress->cancel();
        m_scanProgress->waitForFinished();
        break;
    default:
        break;
    }
    m_state = QnManualCameraSearchStatus::Aborted;
}

QnManualCameraSearchStatus QnManualCameraSearcher::status() const {
    QMutexLocker lock(&m_mutex);
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        return QnManualCameraSearchStatus(m_state, 1, 1);
    case QnManualCameraSearchStatus::CheckingOnline:
        if (!m_onlineHosts)
            return QnManualCameraSearchStatus(m_state, 0, 1);
        return QnManualCameraSearchStatus(m_state, m_onlineHosts->progressValue(), m_onlineHosts->progressMaximum() - m_onlineHosts->progressMinimum());
    case QnManualCameraSearchStatus::CheckingHost:
    {
        if (!m_scanProgress)
            return QnManualCameraSearchStatus(m_state, 0, 1);
        int divider = PluginsEnumerator::pluginsCount();
        return QnManualCameraSearchStatus(m_state,
                                          m_scanProgress->progressValue() / divider,
                                          (m_scanProgress->progressMaximum() - m_scanProgress->progressMinimum()) / divider);
    }
    default:
        break;
    }
    return QnManualCameraSearchStatus(m_state, 0, 1);
}

QnManualCameraSearchCameraList QnManualCameraSearcher::results() const {
    QnManualCameraSearchCameraList results;

    QMutexLocker lock(&m_mutex);
    switch(m_state) {
    case QnManualCameraSearchStatus::CheckingHost:
        if (m_scanProgress) {
            for (int i = 0; i < m_scanProgress->resultCount(); ++i) {
                if (!m_scanProgress->isResultReadyAt(i))
                    continue;
                results << m_scanProgress->resultAt(i);
            }
        }
        break;
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        return m_results;
    default:
        break;
    }
    return results;
}
