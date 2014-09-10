#ifndef MAIN_H
#define MAIN_H

#include <QtCore/QTimer>
#include <QtCore/QStringList>

#include <api/common_message_processor.h>
#include <business/business_fwd.h>
#include <core/resource/resource_fwd.h>

#include "http/progressive_downloading_server.h"
#include "network/universal_tcp_listener.h"

#include "utils/common/long_runnable.h"
#include "nx_ec/impl/ec_api_impl.h"
#include "utils/common/public_ip_discovery.h"


class QnAppserverResourceProcessor;
class QNetworkReply;
class QnServerMessageProcessor;
struct QnModuleInformation;
class QnModuleFinder;
struct QnPeerRuntimeInfo;

class QnMain : public QnLongRunnable
{
    Q_OBJECT

public:
    QnMain(int argc, char* argv[]);
    ~QnMain();

    void stopObjects();
    void run();
public slots:
    void stopAsync();
    void stopSync();
private slots:
    void loadResourcesFromECS(QnCommonMessageProcessor* messageProcessor);
    void at_localInterfacesChanged();
    void at_serverSaved(int, ec2::ErrorCode err);
    void at_cameraIPConflict(const QHostAddress& host, const QStringList& macAddrList);
    void at_storageManager_noStoragesAvailable();
    void at_storageManager_storageFailure(const QnResourcePtr& storage, QnBusiness::EventReason reason);
    void at_storageManager_rebuildFinished();
    void at_timer();
    void at_connectionOpened();
    void at_license_issue();

    void at_appStarted();
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo);
    void at_emptyDigestDetected(const QnUserResourcePtr& user, const QString& login, const QString& password);
    void at_restartServerRequired();
    void at_updatePublicAddress(const QHostAddress& publicIP);
private:
    void updateDisabledVendorsIfNeeded();
    void updateAllowCameraCHangesIfNeed();
    bool initTcpListener();
    QHostAddress getPublicAddress();
    QnMediaServerResourcePtr findServer(ec2::AbstractECConnectionPtr ec2Connection, Qn::PanicMode* pm);
private:
    int m_argc;
    char** m_argv;
    bool m_startMessageSent;
    qint64 m_firstRunningTime;

    QnModuleFinder* m_moduleFinder;
    QnUniversalTcpListener* m_universalTcpListener;
    QnMediaServerResourcePtr m_mediaServer;
    QSet<QUuid> m_updateUserRequests;
    QHostAddress m_publicAddress;
    std::unique_ptr<QnPublicIPDiscovery> m_ipDiscovery;
    std::unique_ptr<QTimer> m_updatePiblicIpTimer;
};

#endif // MAIN_H
