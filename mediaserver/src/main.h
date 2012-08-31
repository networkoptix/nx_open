#ifndef MAIN_H
#define MAIN_H

#include "utils/common/longrunnable.h"
#include "core/resource/video_server_resource.h"
#include "http/progressive_downloading_server.h"
#include "network/universal_tcp_listener.h"

class QnAppserverResourceProcessor;
class QnRtspListener;
class QnRestServer;

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

private:
    int m_argc;
    char** m_argv;

    QnAppserverResourceProcessor* m_processor;
    QnRtspListener* m_rtspListener;
    QnRestServer* m_restServer;
    QnProgressiveDownloadingServer* m_progressiveDownloadingServer;
    QnUniversalTcpListener* m_universalTcpListener;
    QnVideoServerResourcePtr m_videoServer;
};

#endif // MAIN_H
