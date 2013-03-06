#ifndef QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
#define QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H

#include "core/resource_managment/resource_discovery_manager.h"

class QnMServerResourceDiscoveryManager: public QnResourceDiscoveryManager
{
public:
    QnMServerResourceDiscoveryManager();
    virtual ~QnMServerResourceDiscoveryManager();

protected:
    virtual bool processDiscoveredResources(QnResourceList& resources) override;

private:

    void check_if_accessible(QnResourceList& justfoundList, int threads, CLNetState& netState);

    void resovle_conflicts(QnResourceList& device_list, const CLIPList& busy_list, bool *ip_finished, CLNetState& netState);


    void markOfflineIfNeeded(QSet<QString>& discoveredResources);

    void updateResourceStatus(QnResourcePtr res, QSet<QString>& discoveredResources);


    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(QnResourcePtr res);
private:
    bool m_foundSmth; // minor just to minimize lof output
    QMap<QString, int> m_resourceDiscoveryCounter;
    QTime netStateTime;
    CLNetState netState;
};

#endif //QN_MSERVER_RESOURCE_DISCOVERY_MANAGER_H
