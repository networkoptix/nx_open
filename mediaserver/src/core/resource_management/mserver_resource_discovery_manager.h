#ifndef QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
#define QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H

#include "core/resource_management/resource_discovery_manager.h"

class QnMServerResourceDiscoveryManager: public QnResourceDiscoveryManager
{
    Q_OBJECT
public:
    QnMServerResourceDiscoveryManager();
    virtual ~QnMServerResourceDiscoveryManager();
signals:
    void cameraDisconnected(QnResourcePtr camera, qint64 timestamp);
protected:
    virtual bool processDiscoveredResources(QnResourceList& resources) override;
private:
    void markOfflineIfNeeded(QSet<QString>& discoveredResources);

    void updateResourceStatus(QnResourcePtr res, QSet<QString>& discoveredResources);


    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(QnResourcePtr res);
    void addNewCamera(QnVirtualCameraResourcePtr cameraResource);
    bool canTakeForeignCamera(QnResourcePtr camera);
private:
    bool m_foundSmth; // minor just to minimize lof output
    QMap<QString, int> m_resourceDiscoveryCounter;
    QMap<QString, int> m_disconnectSended;
    QTime netStateTime;
    CLNetState netState;
};

#endif //QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
