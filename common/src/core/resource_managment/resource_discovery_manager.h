#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <QtCore/QThread>
#include <QAuthenticator>
#include "utils/common/long_runnable.h"
#include "utils/network/netstate.h"
#include "core/resource/resource.h"
#include "utils/network/nettools.h"

class QnAbstractResourceSearcher;
class QnAbstractDTSSearcher;

struct QnManualCameraInfo
{
    QnManualCameraInfo(const QUrl& url, const QAuthenticator& auth, const QString& resType);
    QList<QnResourcePtr> checkHostAddr() const;

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

    QnResourceDiscoveryManager();
    ~QnResourceDiscoveryManager();

    static QnResourceDiscoveryManager* instance();

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceServer(QnAbstractResourceSearcher* serv);
    void addDTSServer(QnAbstractDTSSearcher* serv);
    void setResourceProcessor(QnResourceProcessor* processor);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual void pleaseStop();

    void setReady(bool ready);

    QnResourceList findResources(QString startAddr, QString endAddr, const QAuthenticator& auth, int port);
    bool registerManualCameras(const QnManualCamerasMap& cameras);
    //QnResourceList processManualAddedResources();
    void setDisabledVendors(const QStringList& vendors);
    bool containManualCamera(const QString& uniqId);

    static void init(QnResourceDiscoveryManager* instance);
protected:
    QMutex m_discoveryMutex;
    unsigned int m_runNumber;
protected:

    virtual void run();
    virtual bool processDiscoveredResources(QnResourceList& resources);
signals:
    void localInterfacesChanged();
    void CameraIPConflict(QHostAddress addr, QStringList macAddrList);
private slots:
    void onInitAsyncFinished(QnResourcePtr res, bool initialized);
    void at_resourceDeleted(const QnResourcePtr& resource);
private:

    void updateLocalNetworkInterfaces();

    // returns new resources( not from pool) or updates some in resource pool
    QnResourceList findNewResources();

    void appendManualDiscoveredResources(QnResourceList& resources);
    void dtsAssignment();
private:
    QMutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;
    QnManualCamerasMap m_manualCameraMap;

    bool m_server;
    volatile bool m_ready;

    QList<QHostAddress> m_allLocalAddresses;


    QVector<QnAbstractDTSSearcher*> m_dstList;
    QSet<QString> m_disabledVendorsForAutoSearch;
    static QnResourceDiscoveryManager* m_instance;
};

#endif //QN_RESOURCE_DISCOVERY_MANAGER_H
