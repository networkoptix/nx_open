#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <QtCore/QThread>
#include <QAuthenticator>
#include "utils/common/longrunnable.h"
#include "utils/network/netstate.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"

class QnAbstractResourceSearcher;

struct QnManualCameraInfo
{
    QnManualCameraInfo(const QHostAddress& addr, int port, const QAuthenticator& auth, const QString& resType);
    QnResourcePtr checkHostAddr() const;

    QHostAddress addr;
    int port;
    QnResourceTypePtr resType;
    QAuthenticator auth;
    QnAbstractResourceSearcher* searcher;
};
typedef QMap<quint32, QnManualCameraInfo> QnManualCamerasMap;

class QnAbstractResourceSearcher;

// this class just searches for new resources
// it uses others plugins
// it puts result into resource pool
class QnResourceDiscoveryManager : public QnLongRunnable, public QnResourceFactory
{
    Q_OBJECT;

public:

    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

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

    QnResourceList findResources(QHostAddress startAddr, QHostAddress endAddr, const QAuthenticator& auth, int port);
    bool registerManualCameras(const QnManualCamerasMap& cameras);
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
    QnResourceList findNewResources();

    void check_if_accessible(QnResourceList& justfoundList, int threads);

    void resovle_conflicts(QnResourceList& device_list, const CLIPList& busy_list, bool *ip_finished);

    
    void markOfflineIfNeeded();

    void updateResourceStatus(QnResourcePtr res);


    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(QnResourcePtr res);
    void appendManualDiscoveredResources(QnResourceList& resources);
private:
    QMutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;
    QnManualCamerasMap m_manualCameraMap;

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
