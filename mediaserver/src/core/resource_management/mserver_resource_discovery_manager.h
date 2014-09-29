#ifndef QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
#define QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H

#include <QtCore/QTime>

#include "core/resource_management/resource_discovery_manager.h"

class QnMServerResourceDiscoveryManager: public QnResourceDiscoveryManager
{
    Q_OBJECT
public:
    QnMServerResourceDiscoveryManager();
    virtual ~QnMServerResourceDiscoveryManager();

    //!Implementation of QnResourceFactory::createResource
    /*!
        Calls \a QnResourceDiscoveryManager::createResource first. If resource was not created and resource type is present in system, returns data-only resource
    */
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;

signals:
    void cameraDisconnected(const QnResourcePtr& camera, qint64 timestamp);

protected:
    virtual bool processDiscoveredResources(QnResourceList& resources) override;

private:
    void markOfflineIfNeeded(QSet<QString>& discoveredResources);

    void updateResourceStatus(const QnResourcePtr& res, QSet<QString>& discoveredResources);

    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(const QnResourcePtr& res);
    void addNewCamera(const QnVirtualCameraResourcePtr& cameraResource);
    bool canTakeForeignCamera(const QnSecurityCamResourcePtr& camera);

private:
    bool m_foundSmth; // minor just to minimize lof output
    QMap<QString, int> m_resourceDiscoveryCounter;
    QMap<QString, int> m_disconnectSended;
    QTime netStateTime;
    CLNetState netState;
    int m_serverOfflineTimeout;
};

#endif //QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
