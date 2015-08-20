#ifndef MEDIA_SERVER_PROCESS_H
#define MEDIA_SERVER_PROCESS_H

#include <memory>

#include <QtCore/QTimer>
#include <QtCore/QStringList>

#include <api/common_message_processor.h>
#include <business/business_fwd.h>
#include <core/resource/resource_fwd.h>
#include <crash_reporter.h>

#include "http/progressive_downloading_server.h"
#include "network/universal_tcp_listener.h"
#include "platform/monitoring/global_monitor.h"

#include "utils/common/long_runnable.h"
#include "nx_ec/impl/ec_api_impl.h"
#include "utils/common/public_ip_discovery.h"
#include <utils/network/http/http_mod_manager.h>
#include <utils/network/upnp/upnp_port_mapper.h>


class QnAppserverResourceProcessor;
class QNetworkReply;
class QnServerMessageProcessor;
struct QnModuleInformation;
class QnModuleFinder;
struct QnPeerRuntimeInfo;

class MediaServerProcess : public QnLongRunnable
{
    Q_OBJECT

public:
    MediaServerProcess(int argc, char* argv[]);
    ~MediaServerProcess();

    void stopObjects();
    void run();

    /** Entry point */
    static int main(int argc, char* argv[]);

public slots:
    void stopAsync();
    void stopSync();

private slots:
    void loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor);
    void at_localInterfacesChanged();
    void at_portMappingChanged(quint16 internalPort, quint16 externalPort, QString externalIp);
    void at_serverSaved(int, ec2::ErrorCode err);
    void at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList);
    void at_storageManager_noStoragesAvailable();
    void at_storageManager_storageFailure(const QnResourcePtr& storage, QnBusiness::EventReason reason);
    void at_storageManager_rebuildFinished();
    void at_timer();
    void at_connectionOpened();
    void at_serverModuleConflict(const QnModuleInformation &moduleInformation, const SocketAddress &address);

    void at_appStarted();
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
    void at_emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);
    void at_databaseDumped();
    void at_systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void at_updatePublicAddress(const QHostAddress& publicIP);

private:
    void updateDisabledVendorsIfNeeded();
    void updateAllowCameraCHangesIfNeed();
    void updateAddressesList();
    bool initTcpListener();
    QHostAddress getPublicAddress();
    QnMediaServerResourcePtr findServer(ec2::AbstractECConnectionPtr ec2Connection);
    void saveStorages(ec2::AbstractECConnectionPtr ec2Connection, const QnStorageResourceList& storages);
    void dumpSystemUsageStats();
    void saveAdminPswdHash();
    bool isStopping() const;

private:
    int m_argc;
    char** m_argv;
    bool m_startMessageSent;
    qint64 m_firstRunningTime;

    QnModuleFinder* m_moduleFinder;
    std::unique_ptr<nx_http::HttpModManager> m_httpModManager;
    QnUniversalTcpListener* m_universalTcpListener;
    QnMediaServerResourcePtr m_mediaServer;
    QSet<QnUuid> m_updateUserRequests;
    QHostAddress m_publicAddress;
    QList<QHostAddress> m_localAddresses;
    std::map<HostAddress, quint16> m_forwardedAddresses;
    QnMutex m_mutex;
    std::unique_ptr<QnPublicIPDiscovery> m_ipDiscovery;
    std::unique_ptr<QTimer> m_updatePiblicIpTimer;
    quint64 m_dumpSystemResourceUsageTaskID;
    bool m_stopping;
    mutable QnMutex m_stopMutex;
    std::unique_ptr<ec2::CrashReporter> m_crashReporter;
};

#endif // MEDIA_SERVER_PROCESS_H
