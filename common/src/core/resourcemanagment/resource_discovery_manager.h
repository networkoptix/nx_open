#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <QtCore/QThread>
#include <QAuthenticator>
#include "utils/common/longrunnable.h"
#include "utils/network/netstate.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"

class QnAbstractResourceSearcher;
class QnAbstractDTSSearcher;

struct QnManualCameraInfo
{
    QnManualCameraInfo(const QUrl& url, const QAuthenticator& auth, const QString& resType);
    QnResourcePtr checkHostAddr() const;

    QUrl url;
    QnResourceTypePtr resType;
    QAuthenticator auth;
    QnAbstractResourceSearcher* searcher;
};
typedef QMap<QString, QnManualCameraInfo> QnManualCamerasMap;

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
    void addDTSServer(QnAbstractDTSSearcher* serv);
    void setResourceProcessor(QnResourceProcessor* processor);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual void pleaseStop();

    void setReady(bool ready);

    QnResourceList findResources(QHostAddress startAddr, QHostAddress endAddr, const QAuthenticator& auth, int port);
    bool registerManualCameras(const QnManualCamerasMap& cameras);
    QnResourceList processManualAddedResources();
protected:
    QnResourceDiscoveryManager();

    virtual void run();

signals:
    void localInterfacesChanged();
private slots:
    void onInitAsyncFinished(QnResourcePtr res, bool initialized);
    void at_resourceDeleted(const QnResourcePtr& resource);
private:
    void updateLocalNetworkInterfaces();

    // returns new resources( not from pool) or updates some in resource pool
    QnResourceList findNewResources();

    void check_if_accessible(QnResourceList& justfoundList, int threads, CLNetState& netState);

    void resovle_conflicts(QnResourceList& device_list, const CLIPList& busy_list, bool *ip_finished, CLNetState& netState);

    
    void markOfflineIfNeeded(QSet<QString>& discoveredResources);

    void updateResourceStatus(QnResourcePtr res, QSet<QString>& discoveredResources);


    // ping resources from time to time to keep OS ARP table updated; speeds up resource (start) time in case if not recorded
    void pingResources(QnResourcePtr res);
    void appendManualDiscoveredResources(QnResourceList& resources);
    bool processDiscoveredResources(QnResourceList& resources, bool doOfflineCheck);
    void dtsAssignment();
private:
    QMutex m_searchersListMutex;
    QMutex m_discoveryMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;
    QnManualCamerasMap m_manualCameraMap;

    bool m_server;
    bool m_foundSmth; // minor just to minimize lof output
    volatile bool m_ready;

    unsigned int m_runNumber;

    QList<QHostAddress> m_allLocalAddresses;


    QMap<QString, int> m_resourceDiscoveryCounter;
    QVector<QnAbstractDTSSearcher*> m_dstList;
	QTime netStateTime;
    QTime netStateTime;
};

#endif //QN_RESOURCE_DISCOVERY_MANAGER_H
