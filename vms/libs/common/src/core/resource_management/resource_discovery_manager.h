#pragma once

#include <memory>
#include <atomic>

#include <QThreadPool>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/url.h>
#include <nx/network/nettools.h>

#include <api/model/manual_camera_seach_reply.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_factory.h>
#include <core/resource/resource_processor.h>

#include <utils/common/connective.h>
#include <nx/utils/log/log.h>
#include <common/common_module_aware.h>
#include "resource_searcher.h"

class QnAbstractResourceSearcher;

struct QnManualCameraInfo
{
    QnManualCameraInfo(const nx::utils::Url& url, const QAuthenticator& auth, const QString& resType, const QString& uniqueId);
    QList<QnResourcePtr> checkHostAddr() const;

    nx::utils::Url url;
    QnResourceTypePtr resType;
    QAuthenticator auth;
    QnAbstractResourceSearcher* searcher;
    QString uniqueId;
    bool isUpdated = false;
};

class QnAbstractResourceSearcher;

class QnResourceDiscoveryManager;
/*!
    This class instance only calls QnResourceDiscoveryManager::doResourceDiscoverIteration from QnResourceDiscoveryManager thread,
    since we cannot move QnResourceDiscoveryManager object to QnResourceDiscoveryManager thread (weird...)
*/
class QnResourceDiscoveryManagerTimeoutDelegate
:
    public QObject
{
    Q_OBJECT

public:
    QnResourceDiscoveryManagerTimeoutDelegate(QnResourceDiscoveryManager* discoveryManager);

public slots:
    void onTimeout();

private:
    QnResourceDiscoveryManager* m_discoveryManager;
};

class CameraDriverRestrictionList;

// this class just searches for new resources
// it uses others plugins
// it puts result into resource pool
class QnResourceDiscoveryManager
:
    public Connective<QnLongRunnable>,
    public QnResourceFactory,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QnLongRunnable>;

public:
    enum State
    {
        InitialSearch,
        PeriodicSearch
    };

    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

    QnResourceDiscoveryManager(QObject* parent);
    virtual ~QnResourceDiscoveryManager() override;

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceServer(QnAbstractResourceSearcher* serv);
    void setResourceProcessor(QnResourceProcessor* processor);
    QnAbstractResourceSearcher* searcherByManufacturer(const QString& manufacturer) const;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual void stop() override;
    virtual void pleaseStop() override;

    void setReady(bool ready);

    /** Returns number of cameras that were successfully added. */
    QSet<QString> registerManualCameras(const std::vector<QnManualCameraInfo>& cameras);
    bool isManuallyAdded(const QnSecurityCamResourcePtr& camera) const;
    QnManualCameraInfo manualCameraInfo(const QnSecurityCamResourcePtr& camera) const;

    ResourceSearcherList plugins() const;

    //!This method MUST be called from non-GUI thread, since it can block for some time
    virtual void doResourceDiscoverIteration();

    State state() const;

    void setLastDiscoveredResources(const QnResourceList& resources);
    QnResourceList lastDiscoveredResources() const;
    void addResourcesImmediatly(QnResourceList& resources);

    static QnNetworkResourcePtr findSameResource(const QnNetworkResourcePtr& netRes);

    QThreadPool* threadPool();
    DiscoveryMode discoveryMode() const;
public slots:
    virtual void start( Priority priority = InheritPriority ) override;

protected:
    virtual void run() override;
    QnManualCameraInfo manualCameraInfoUnsafe(const QnSecurityCamResourcePtr& camera) const;

signals:
    void localSearchDone();
    void localInterfacesChanged();
    void CameraIPConflict(QHostAddress addr, QStringList macAddrList);

protected:
    enum class SearchType
    {
        Full,
        Partial
    };
    virtual bool processDiscoveredResources(QnResourceList& resources, SearchType searchType);
    bool canTakeForeignCamera(const QnSecurityCamResourcePtr& camera, int awaitingToMoveCameraCnt);

    virtual nx::vms::common::AnalyticsPluginResourcePtr
        createAnalyticsPluginResource(const QnResourceParams& params);

    virtual nx::vms::common::AnalyticsEngineResourcePtr
        createAnalyticsEngineResource(const QnResourceParams& params);

    friend class QnResourceDiscoveryManagerTimeoutDelegate;

    void updateLocalNetworkInterfaces();

    // Returns new resources or updates some in resource pool.
    QnResourceList findNewResources();
    // Run search of local files.
    void doInitialSearch();

    void appendManualDiscoveredResources(QnResourceList& resources);

    void updateSearcherUsageUnsafe(QnAbstractResourceSearcher *searcher, bool usePartialEnable);
    void updateSearchersUsage();
    bool isRedundancyUsing() const;

protected:
    QThreadPool m_threadPool;

    mutable QnMutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;
    QMap<QString, QnManualCameraInfo> m_manualCameraByUniqueId;

    bool m_server;
    std::atomic<bool> m_ready;

    QList<QHostAddress> m_allLocalAddresses;

    QScopedPointer<QTimer> m_timer;
    State m_state;
    QSet<QString> m_recentlyDeleted;

    QHash<QnUuid, QnManualResourceSearchStatus> m_searchProcessStatuses;
    QHash<QnUuid, QnManualResourceSearchList> m_searchProcessResults; // TODO: #wearable unused!!!

    mutable QnMutex m_resListMutex;
    QnResourceList m_lastDiscoveredResources[6];
    int m_discoveryUpdateIdx;

protected:
    unsigned int m_runNumber;
    int m_serverOfflineTimeout;
};
