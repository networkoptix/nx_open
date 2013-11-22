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
        QnManualCameraSearchCameraList result;
        foreach(const QnResourcePtr &resource, plugin->checkHostAddr(url, auth, true))
            result << fromResource(resource);
        return result;
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

    QUrl url;
    QAuthenticator auth;
};

void reduceFunction(QnManualCameraSearchCameraList &finalResult, const QnManualCameraSearchCameraList &intermediateResult) {
    finalResult << intermediateResult;
}

QnManualCameraSearcher::QnManualCameraSearcher():
    m_state(QnManualCameraSearchStatus::Init),
    m_onlineHosts(NULL),
    m_results(NULL)
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
        {
            QN_INCREASE_MAX_THREADS(qMin(endIPv4Addr - startIPv4Addr, onlineScanThreadCount));
            {
                QMutexLocker lock(&m_mutex);
                if (m_state == QnManualCameraSearchStatus::Aborted)
                    return;
                m_state = QnManualCameraSearchStatus::CheckingOnline;
                QFuture<QStringList> future = QnIpRangeChecker::onlineHostsAsync(QHostAddress(startAddr), QHostAddress(endAddr), port ? port : 80);
                m_onlineHosts = &future;
            }
            onlineHosts = m_onlineHosts->result(); //here we are waiting for the results
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
        QMutexLocker lock(&m_mutex);
        if (m_state == QnManualCameraSearchStatus::Aborted)
            return;
        QFuture<QnManualCameraSearchCameraList> results = QtConcurrent::mappedReduced(checkers, &SinglePluginChecker::mapFunction, &reduceFunction);
        m_results = &results;
    }
    m_results->result();
    {
         QMutexLocker lock(&m_mutex);
         if (m_state == QnManualCameraSearchStatus::Aborted)
             return;
         m_state = QnManualCameraSearchStatus::Finished;
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
        if (!m_results)
            return;
        m_results->cancel();
        m_results->waitForFinished();
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
        if (!m_results)
            return QnManualCameraSearchStatus(m_state, 0, 1);
        return QnManualCameraSearchStatus(m_state, m_results->progressValue(), m_results->progressMaximum() - m_results->progressMinimum());
    default:
        break;
    }
    return QnManualCameraSearchStatus(m_state, 0, 1);
}

QnManualCameraSearchCameraList QnManualCameraSearcher::results() const {
    QMutexLocker lock(&m_mutex);
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        if (m_results && !m_results->isRunning())
            return m_results->result();
        break;
    default:
        break;
    }
    return QnManualCameraSearchCameraList();
}
