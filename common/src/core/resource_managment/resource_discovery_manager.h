#ifndef QN_RESOURCE_DISCOVERY_MANAGER_H
#define QN_RESOURCE_DISCOVERY_MANAGER_H

#include <memory> // for auto_ptr

#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QAuthenticator>

#include <api/media_server_cameras_data.h>

#include <core/resource/resource.h>

#include <utils/common/long_runnable.h>
#include <utils/network/netstate.h>
#include <utils/network/nettools.h>

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
class QnResourceDiscoveryManager : public QnLongRunnable, public QnResourceFactory
{
    Q_OBJECT;

public:
    enum State
    {
        initialSearch,
        periodicSearch
    };


    typedef QList<QnAbstractResourceSearcher*> ResourceSearcherList;

    QnResourceDiscoveryManager( const CameraDriverRestrictionList* cameraDriverRestrictionList = NULL );
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

    void searchResources(const QUuid &processUuid, const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port);
    bool registerManualCameras(const QnManualCamerasMap& cameras);
    //QnResourceList processManualAddedResources();
    void setDisabledVendors(const QStringList& vendors);
    bool containManualCamera(const QString& uniqId);

    bool getSearchStatus(const QUuid &searchProcessUuid, QnManualCameraSearchStatus &status);
    void setSearchStatus(const QUuid &searchProcessUuid, const QnManualCameraSearchStatus &status);

    QnManualCameraSearchCameraList getSearchResults(const QUuid &searchProcessUuid);
    void setSearchResults(const QUuid &searchProcessUuid, const QnManualCameraSearchCameraList &results);

    void clearSearch(const QUuid &searchProcessUuid);

    //!This method MUST be called from non-GUI thread, since it can block for some time
    void doResourceDiscoverIteration();

    static void init(QnResourceDiscoveryManager* instance);
    State state() const;
public slots:
    virtual void start( Priority priority = InheritPriority ) override;

protected:
    QMutex m_discoveryMutex;

    /** Mutex that is used to synchronize access to manual camera addition progress. */
    QMutex m_searchProcessMutex;

    unsigned int m_runNumber;

    virtual void run();
    virtual bool processDiscoveredResources(QnResourceList& resources);

signals:
    void localSearchDone();
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

    std::auto_ptr<QTimer> m_timer;
    State m_state;
    QSet<QString> m_recentlyDeleted;
    const CameraDriverRestrictionList* m_cameraDriverRestrictionList;

    QHash<QUuid, QnManualCameraSearchStatus> m_searchProcessStatuses;
    QHash<QUuid, QnManualCameraSearchCameraList> m_searchProcessResults;
};

#endif //QN_RESOURCE_DISCOVERY_MANAGER_H
