// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QTimer>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource/resource_factory.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>
#include <nx/network/rest/audit.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/vms/common/system_context_aware.h>

#include "resource_searcher.h"

class QnAbstractResourceSearcher;

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

// this class just searches for new resources
// it uses others plugins
// it puts result into resource pool
class NX_VMS_COMMON_API QnResourceDiscoveryManager:
    public QnLongRunnable,
    public QnResourceFactory,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QnLongRunnable;

public:
    enum State
    {
        InitialSearch,
        PeriodicSearch
    };

    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

    QnResourceDiscoveryManager(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnResourceDiscoveryManager() override;

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceSearcher(QnAbstractResourceSearcher* searcher, bool pushFront = false);
    void setResourceProcessor(QnResourceProcessor* processor);
    QnAbstractResourceSearcher* searcherByManufacturer(const QString& manufacturer) const;

    virtual QnResourcePtr createResource(const nx::Uuid &resourceTypeId, const QnResourceParams& params) override;

    virtual void stop() override;
    virtual void pleaseStop() override;

    void setReady(bool ready);

    ResourceSearcherList plugins() const;

    //!This method MUST be called from non-GUI thread, since it can block for some time
    virtual void doResourceDiscoverIteration();

    State state() const;

    void setLastDiscoveredResources(const QnResourceList& resources);
    QnResourceList lastDiscoveredResources() const;

    QThreadPool* threadPool();
    DiscoveryMode discoveryMode() const;
public slots:
    virtual void start( Priority priority = InheritPriority ) override;

protected:
    virtual void run() override;

signals:
    void localSearchDone();
    void localInterfacesChanged();

protected:
    /** Allows Server's descendant to trigger specific behavior on Edge devices. */
    virtual bool isEdgeServer() const { return false; }

    enum class SearchType
    {
        Full,
        Partial
    };
    virtual bool processDiscoveredResources(QnResourceList& resources, SearchType searchType);

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

    virtual void appendManualDiscoveredResources(QnResourceList&) {}

    void updateSearcherUsageUnsafe(QnAbstractResourceSearcher *searcher, bool usePartialEnable);
    void updateSearchersUsage();
    bool isRedundancyUsing() const;
    virtual QnResourceList remapPhysicalIdIfNeed(const QnResourceList& resources);

    /**
     * Intended to prevent the camera drivers from discovering cameras which are supposed to be
     * discovered by different drivers, or which must not be discovered due to other reasons.
     */
    virtual bool isCameraAllowed(
        const QString& driverName,
        const QnVirtualCameraResource* securityCamResource) const;

protected:
    QThreadPool m_threadPool;

    mutable nx::Mutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;

    bool m_server;
    std::atomic<bool> m_ready;

    std::optional<QList<nx::network::HostAddress>> m_allLocalAddresses;

    QScopedPointer<QTimer> m_timer;
    State m_state;

    // Ignore resources for 1 discovery loop.
    QSet<QString> m_resourcesToIgnore;

    mutable nx::Mutex m_resListMutex;
    std::array<QnResourceList, 6> m_lastDiscoveredResources;
    int m_discoveryUpdateIdx;

protected:
    unsigned int m_runNumber;
};
