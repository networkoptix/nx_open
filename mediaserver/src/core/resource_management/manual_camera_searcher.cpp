
#include "manual_camera_searcher.h"

#include <type_traits>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentMap>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/log.h>
#include <utils/network/ip_range_checker.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_searcher.h>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>




static const int MAX_PERCENT = 100;
//assuming that scanning ports is ten times faster than plugin check
static const int PORT_SCAN_MAX_PROGRESS_PERCENT = 10;
static_assert( PORT_SCAN_MAX_PROGRESS_PERCENT < MAX_PERCENT, "PORT_SCAN_MAX_PROGRESS_PERCENT must be less than MAX_PERCENT" );

namespace {
    /**
     * @brief resourceExistsInPool          Check if found camera is already exists in pool by its unique ID. For onvif cameras host is also checked.
     * @param resource                      Camera resource.
     * @return                              True if resource is already in resource pool, false otherwise.
     */
    bool resourceExistsInPool(const QnResourcePtr &resource) {
        bool existResource = false;
        if (qnResPool->hasSuchResource(resource->getUniqueId())) { 
            existResource = true; // already in resource pool 
        }
        else {
            // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition checking for IP address
            QnNetworkResourcePtr netRes = resource.dynamicCast<QnNetworkResource>();
            if (netRes) {
                QnNetworkResourceList existResList = qnResPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
                foreach(QnNetworkResourcePtr existRes, existResList) 
                {
                    QnVirtualCameraResourcePtr existCam = existRes.dynamicCast<QnVirtualCameraResource>();
                    if (!existCam)
                        continue;
                    if (!existCam->isManuallyAdded())
                        existResource = true; // block manual and auto add in same time
                    else if (existRes->getTypeId() != netRes->getTypeId()) 
                    {
                        // allow several manual cameras on the same IP if cameras have different ports
                        QUrl url1(existRes->getUrl());
                        QUrl url2(netRes->getUrl());
                        if (url1.port() == url2.port())
                            existResource = true; // camera found by different drivers on the same port
                    }
                }
            }
        }
        return existResource;
    }

    QnManualCameraSearchSingleCamera fromResource(const QnResourcePtr &resource) 
    {
        QnSecurityCamResourcePtr cameraResource = resource.dynamicCast<QnSecurityCamResource>();
        return QnManualCameraSearchSingleCamera(resource->getName(),
                                                resource->getUrl(),
                                                qnResTypePool->getResourceType(resource->getTypeId())->getName(),
                                                cameraResource->getVendor(),
                                                resourceExistsInPool(resource));
    }
}

struct SinglePluginChecker {

    SinglePluginChecker(QnAbstractNetworkResourceSearcher* plugin, const QUrl &url, const QAuthenticator &auth):
        plugin(plugin), url(url), auth(auth) {}


    QnManualCameraSearchCameraList mapFunction() const {
        QnManualCameraSearchCameraList results;
        foreach(const QnResourcePtr &resource, plugin->checkHostAddr(url, auth, true))
        {
            QnSecurityCamResourcePtr camRes = resource.dynamicCast<QnSecurityCamResource>();
            //checking, if found resource is reserved by some other searcher
            if( camRes &&
                !CameraDriverRestrictionList::instance()->driverAllowedForCamera( plugin->manufacture(), camRes->getVendor(), camRes->getModel() ) )
            {
                continue;   //camera is not allowed to be used with this driver
            }
            results << fromResource(resource);
        }
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
    m_scanProgress(NULL),
    m_cancelled(false),
    m_hostRangeSize(0)
{
}

QnManualCameraSearcher::~QnManualCameraSearcher() {
}

bool QnManualCameraSearcher::run( QThreadPool* threadPool, const QString &startAddr, const QString &endAddr, const QAuthenticator &auth, int port ) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == QnManualCameraSearchStatus::Aborted)
            return false;
    }

    QList<SinglePluginChecker> checkers;

    m_singleAddressCheck = endAddr.isNull();
    if (m_singleAddressCheck) {
        checkers << PluginsEnumerator(startAddr, port, auth).enumerate();
    } else { //subnet scan
        const quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
        const quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();
        if (endIPv4Addr < startIPv4Addr)
            return false;

        QStringList onlineHosts;
        {
            {
                QMutexLocker lock(&m_mutex);
                m_hostRangeSize = endIPv4Addr - startIPv4Addr;
                m_state = QnManualCameraSearchStatus::CheckingOnline;
            }

            onlineHosts = m_ipChecker.onlineHosts(
                QHostAddress(startAddr),
                QHostAddress(endAddr),
                port ? port : nx_http::DEFAULT_HTTP_PORT );

            {
                QMutexLocker lock(&m_mutex);
                if( m_cancelled )
                {
                    m_state = QnManualCameraSearchStatus::Aborted;
                    return false;
                }
            }
        }

        foreach(const QString& addr, onlineHosts)
            checkers << PluginsEnumerator(addr, port, auth).enumerate();
    }

    m_state = QnManualCameraSearchStatus::CheckingHost;

    {
        QnConcurrent::QnFuture<QnManualCameraSearchCameraList> results;
        {
            QMutexLocker lock(&m_mutex);
            if (m_state == QnManualCameraSearchStatus::Aborted)
                return false;
            m_state = QnManualCameraSearchStatus::CheckingHost;
            QnScopedThreadRollback ensureFreeThread( 1 );
            results = QnConcurrent::mapped( threadPool, checkers, std::mem_fn(&SinglePluginChecker::mapFunction) );
            m_scanProgress = &results;
        }
        results.waitForFinished();
        {
             QMutexLocker lock(&m_mutex);
             for (size_t i = 0; i < results.resultCount(); ++i) {
                 if (!results.isResultReadyAt(i))
                     continue;
                 m_results << results.resultAt(i);
             }
             m_scanProgress = NULL;
             if (results.isCanceled())
                 m_state = QnManualCameraSearchStatus::Aborted;
             else
                 m_state = QnManualCameraSearchStatus::Finished;
        }
    }

    return true;
}

void QnManualCameraSearcher::cancel() {
    QMutexLocker lock(&m_mutex);
    m_cancelled = true;
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
        return;
    case QnManualCameraSearchStatus::CheckingOnline:
        m_ipChecker.pleaseStop();
        m_ipChecker.join();
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
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const {
    QMutexLocker lock(&m_mutex);

    QnManualCameraSearchProcessStatus result;

    // filling cameras field
    switch(m_state) {
    case QnManualCameraSearchStatus::CheckingHost:
        if (m_scanProgress) {
            for (size_t i = 0; i < m_scanProgress->resultCount(); ++i) {
                if (!m_scanProgress->isResultReadyAt(i))
                    continue;
                result.cameras << m_scanProgress->resultAt(i);
            }
        }
        break;
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        result.cameras = m_results;
        break;
    default:
        break;
    }

    // filling state field
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        result.status = QnManualCameraSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
        NX_LOG( lit(" -----------------1 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        break;
    case QnManualCameraSearchStatus::CheckingOnline:
        //considering it to be half of entire job
        result.status = QnManualCameraSearchStatus( m_state, (int)m_ipChecker.hostsChecked()*PORT_SCAN_MAX_PROGRESS_PERCENT/m_hostRangeSize, MAX_PERCENT );
        NX_LOG( lit(" -----------------2 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        break;
    case QnManualCameraSearchStatus::CheckingHost:
    {
        if (!m_scanProgress) {
            result.status = QnManualCameraSearchStatus(m_state, PORT_SCAN_MAX_PROGRESS_PERCENT, MAX_PERCENT);
            NX_LOG( lit(" -----------------3 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        } else {
            const size_t maxProgress = m_scanProgress->progressMaximum() - m_scanProgress->progressMinimum();
            Q_ASSERT( m_scanProgress->progressMaximum() >= m_scanProgress->progressMinimum() );
            const size_t currentProgress = m_scanProgress->progressValue();
            //considering it to be second half of entire job
            result.status = QnManualCameraSearchStatus(
                m_state,
                PORT_SCAN_MAX_PROGRESS_PERCENT + currentProgress*(MAX_PERCENT - PORT_SCAN_MAX_PROGRESS_PERCENT)/(maxProgress > 0 ? maxProgress : currentProgress),
                MAX_PERCENT );
            NX_LOG( lit(" -----------------4 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        }
        break;
    }
    default:
        result.status = QnManualCameraSearchStatus(m_state, 0, MAX_PERCENT);
        break;
    }
    return result;
}
