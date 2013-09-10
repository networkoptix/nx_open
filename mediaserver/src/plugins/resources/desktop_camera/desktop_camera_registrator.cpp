#include "desktop_camera_registrator.h"
#include "utils/network/tcp_connection_priv.h"
#include "desktop_camera_resource_searcher.h"
#include "utils/network/tcp_listener.h"

class QnDesktopCameraRegistratorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
};

QnDesktopCameraRegistrator::QnDesktopCameraRegistrator(AbstractStreamSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnDesktopCameraRegistratorPrivate, socket, _owner->getOpenSSLContext())
{

}

void QnDesktopCameraRegistrator::run()
{
    Q_D(QnDesktopCameraRegistrator);

    parseRequest();
    sendResponse("HTTP", 200, QByteArray());
    QString userName = d->requestHeaders.value("user-name");
    QnDesktopCameraResourceSearcher::instance().registerCamera(d->socket, userName);
        d->socket = 0; // remove ownership from socket
}
