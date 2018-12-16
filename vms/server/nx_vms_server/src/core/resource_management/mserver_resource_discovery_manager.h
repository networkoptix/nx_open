#pragma once

#include <QtCore/QTime>
#include <QtCore/QElapsedTimer>

#include "core/resource_management/resource_discovery_manager.h"
#include <nx/vms/server/server_module_aware.h>

class QnMServerResourceDiscoveryManager:
    public QnResourceDiscoveryManager
{
    Q_OBJECT
public:
    typedef QnResourceDiscoveryManager base_type;

    QnMServerResourceDiscoveryManager(QnMediaServerModule* serverModule);
    virtual ~QnMServerResourceDiscoveryManager();

    //!Implementation of QnResourceFactory::createResource
    /*!
        Calls \a QnResourceDiscoveryManager::createResource first. If resource was not created and resource type is present in system, returns data-only resource
    */
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;

    static bool hasIpConflict(const QSet<QnNetworkResourcePtr>& cameras);

signals:
    void cameraDisconnected(const QnResourcePtr& camera, qint64 timestamp);

protected:
    virtual bool processDiscoveredResources(QnResourceList& resources, SearchType searchType) override;

    virtual nx::vms::common::AnalyticsPluginResourcePtr
        createAnalyticsPluginResource(const QnResourceParams& params) override;

    virtual nx::vms::common::AnalyticsEngineResourcePtr
        createAnalyticsEngineResource(const QnResourceParams& params) override;

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceDeleted(const QnResourcePtr& resource);

    void markOfflineIfNeeded(QSet<QString>& discoveredResources);

    void updateResourceStatus(const QnNetworkResourcePtr& rpNetRes);

    bool shouldAddNewlyDiscoveredResource(const QnNetworkResourcePtr& newResource) const;

    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(const QnResourcePtr& res);
    void addNewCamera(const QnVirtualCameraResourcePtr& cameraResource);
    void sortForeignResources(QList<QnSecurityCamResourcePtr>& foreignResources);
private:
    //map<uniq id, > TODO #ak old values from this dictionary are not cleared
    QnMediaServerModule* m_serverModule = nullptr;
    QMap<QString, int> m_resourceDiscoveryCounter;
    //map<uniq id, > TODO #ak old values from this dictionary are not cleared
    QMap<QString, int> m_disconnectSended;

    using ResourceList = QMap<QString, QnSecurityCamResourcePtr>;
    QVector<ResourceList> m_tmpForeignResources;
    QnMutex m_discoveryMutex;
    QElapsedTimer m_startupTimer;
    int m_discoveryCounter = 0;
};
