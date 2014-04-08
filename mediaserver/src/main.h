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
#include "utils/network/networkoptixmodulerevealcommon.h"


class QnAppserverResourceProcessor;
class QnRtspListener;
class QnRestServer;
class QNetworkReply;
class NetworkOptixModuleFinder;
class QnServerMessageProcessor;

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
    void at_cameraIPConflict(QHostAddress host, QStringList macAddrList);
    void at_storageManager_noStoragesAvailable();
    void at_storageManager_storageFailure(QnResourcePtr storage, QnBusiness::EventReason reason);
    void at_storageManager_rebuildFinished();
    void at_timer();
    void at_connectionOpened();

    void at_peerFound(
        const QString& moduleType,
        const QString& moduleVersion,
        const QString& systemName,
        const TypeSpecificParamMap& moduleParameters,
        const QString& localInterfaceAddress,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& moduleSeed );
    void at_peerLost(
        const QString& moduleType,
        const TypeSpecificParamMap& moduleParameters,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& moduleSeed );

    void at_appStarted();
private:
    void updateDisabledVendorsIfNeeded();
    void initTcpListener();
    QHostAddress getPublicAddress();
private:
    int m_argc;
    char** m_argv;
    qint64 m_firstRunningTime;

    NetworkOptixModuleFinder* m_moduleFinder;
    QnRtspListener* m_rtspListener;
    QnRestServer* m_restServer;
    QnProgressiveDownloadingServer* m_progressiveDownloadingServer;
    QnUniversalTcpListener* m_universalTcpListener;
    QnMediaServerResourcePtr m_mediaServer;
};

#endif // MAIN_H
