#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <memory> // for auto_ptr

#include <utils/thread/mutex.h>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QAuthenticator>

#include <utils/common/long_runnable.h>
#include <utils/common/singleton.h>
#include <utils/network/netstate.h>
#include <utils/network/nettools.h>

#include <api/model/manual_camera_seach_reply.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_factory.h>
#include <core/resource/resource_processor.h>

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
typedef QMap<QString, QnManualCameraInfo> QnManualCameraInfoMap;

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
    QnResourceDiscoveryManagerTimeoutDelegate( QnResourceDiscoveryManager* discoveryManager );

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
    public QnLongRunnable,
    public QnResourceFactory,
    public Singleton<QnResourceDiscoveryManager>
{
    Q_OBJECT

public:
    enum State
    {
        InitialSearch,
        PeriodicSearch
    };


    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

    QnResourceDiscoveryManager();
    ~QnResourceDiscoveryManager();

    // this function returns only new devices( not in all_devices list);
    //QnResourceList result();
    void addDeviceServer(QnAbstractResourceSearcher* serv);
    void addDTSServer(QnAbstractDTSSearcher* serv);
    void setResourceProcessor(QnResourceProcessor* processor);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual void pleaseStop();

    void setReady(bool ready);

    bool registerManualCameras(const QnManualCameraInfoMap& cameras);
    bool containManualCamera(const QString& url);
    void fillManualCamInfo(QnManualCameraInfoMap& cameras, const QnSecurityCamResourcePtr& camera);

    ResourceSearcherList plugins() const;

    //!This method MUST be called from non-GUI thread, since it can block for some time
    void doResourceDiscoverIteration();

    State state() const;
    
    void setLastDiscoveredResources(const QnResourceList& resources);
    QSet<QString> lastDiscoveredIds() const;
public slots:
    virtual void start( Priority priority = InheritPriority ) override;

protected:
    QnMutex m_discoveryMutex;

    unsigned int m_runNumber;

    virtual void run();
    virtual bool processDiscoveredResources(QnResourceList& resources);

signals:
    void localSearchDone();
    void localInterfacesChanged();
    void CameraIPConflict(QHostAddress addr, QStringList macAddrList);

protected slots:
    void at_resourceDeleted(const QnResourcePtr& resource);
    void at_resourceAdded(const QnResourcePtr& resource);
protected:
    bool canTakeForeignCamera(const QnSecurityCamResourcePtr& camera, int awaitingToMoveCameraCnt);
private:
    void updateLocalNetworkInterfaces();

    // returns new resources( not from pool) or updates some in resource pool
    QnResourceList findNewResources();

    void appendManualDiscoveredResources(QnResourceList& resources);
    void dtsAssignment();

    void updateSearcherUsage(QnAbstractResourceSearcher *searcher, bool usePartialEnable);
    void updateSearchersUsage();
    bool isRedundancyUsing() const;
private:
    QnMutex m_searchersListMutex;
    ResourceSearcherList m_searchersList;
    QnResourceProcessor* m_resourceProcessor;
    QnManualCameraInfoMap m_manualCameraMap;

    bool m_server;
    volatile bool m_ready;

    QList<QHostAddress> m_allLocalAddresses;

    QVector<QnAbstractDTSSearcher*> m_dstList;

    std::auto_ptr<QTimer> m_timer;
    State m_state;
    QSet<QString> m_recentlyDeleted;

    QHash<QnUuid, QnManualCameraSearchStatus> m_searchProcessStatuses;
    QHash<QnUuid, QnManualCameraSearchCameraList> m_searchProcessResults;

    mutable QnMutex m_resListMutex;
    QnResourceList m_lastDiscoveredResources[6];
    int m_discoveryUpdateIdx;
protected:
    int m_serverOfflineTimeout;
};

#endif //QN_RESOURCE_DISCOVERY_MANAGER_H
