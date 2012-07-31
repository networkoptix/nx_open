#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <QtCore/QThread>
#include "utils/common/longrunnable.h"
#include "utils/network/netstate.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"


class QnAbstractResourceSearcher;

// this class just searches for new resources
// it uses others plugins
// it puts result into resource pool
class QnResourceDiscoveryManager : public CLLongRunnable, public QnResourceFactory
{
    Q_OBJECT;

    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

public:
    ~QnResourceDiscoveryManager();

    static QnResourceDiscoveryManager& instance();

    void setServer(bool serv);
    bool isServer() const;

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceServer(QnAbstractResourceSearcher* serv);
    void setResourceProcessor(QnResourceProcessor* processor);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual void pleaseStop();

    CLNetState& getNetState()
    {
        return m_netState;
    }

    void setReady(bool ready);

protected:
    QnResourceDiscoveryManager();

    virtual void run();

signals:
    void localInterfacesChanged();
private slots:
    void onInitAsyncFinished(QnResourcePtr res, bool initialized);
private:
    void updateLocalNetworkInterfaces();

    // returns new resources( not from pool) or updates some in resource pool
    QnResourceList findNewResources(bool *ip_finished);

    void check_if_accessible(QnResourceList& justfoundList, int threads);

    void resovle_conflicts(QnResourceList& device_list, const CLIPList& busy_list, bool *ip_finished);

    
    void markOfflineIfNeeded();

    void updateResourceStatus(QnResourcePtr res);


    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(QnResourcePtr res);

private:
    QMutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;

    CLNetState m_netState;

    bool m_server;
    bool m_foundSmth; // minor just to minimize lof output
    volatile bool m_ready;

    unsigned int m_runNumber;

    QList<QHostAddress> m_allLocalAddresses;


    QSet<QString> m_discoveredResources;
    QMap<QString, int> m_resourceDiscoveryCounter;

};

#endif //QN_RESOURCE_DISCOVERY_MANAGER_H
