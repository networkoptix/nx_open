#ifndef MAIN_H
#define MAIN_H

#include <QTimer>
#include "utils/common/long_runnable.h"
#include "core/resource/media_server_resource.h"
#include "http/progressive_downloading_server.h"
#include "network/universal_tcp_listener.h"


class QnAppserverResourceProcessor;
class QnRtspListener;
class QnRestServer;
class QNetworkReply;

class QnMain : public QnLongRunnable
{
    Q_OBJECT

public:
    QnMain(int argc, char* argv[]);
    ~QnMain();

    void stopObjects();
    void run();

private slots:
    void loadResourcesFromECS();
    void at_localInterfacesChanged();
    void at_serverSaved(int, const QByteArray&, const QnResourceList&, int);
    void at_cameraIPConflict(QHostAddress host, QStringList macAddrList);
    void at_noStorages();
    void at_timer();
private:
    void initTcpListener();
    QHostAddress getPublicAddress();
private:
    int m_argc;
    char** m_argv;
    bool m_waitExtIpFinished;

    QnAppserverResourceProcessor* m_processor;
    QnRtspListener* m_rtspListener;
    QnRestServer* m_restServer;
    QnProgressiveDownloadingServer* m_progressiveDownloadingServer;
    QnUniversalTcpListener* m_universalTcpListener;
    QnMediaServerResourcePtr m_mediaServer;
    QTimer m_timer;
};

#endif // MAIN_H
