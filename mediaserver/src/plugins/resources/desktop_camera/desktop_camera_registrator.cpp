
#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_registrator.h"
#include "utils/network/tcp_connection_priv.h"
#include "desktop_camera_resource_searcher.h"
#include "utils/network/tcp_listener.h"

class QnDesktopCameraRegistratorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
};

QnDesktopCameraRegistrator::QnDesktopCameraRegistrator(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnDesktopCameraRegistratorPrivate, socket)
{
    Q_UNUSED(_owner)
}

void QnDesktopCameraRegistrator::run()
{
    Q_D(QnDesktopCameraRegistrator);

    parseRequest();
    sendResponse("HTTP", 200, QByteArray());
    const QByteArray& userName = nx_http::getHeaderValue(d->request.headers, "user-name");
    if (QnDesktopCameraResourceSearcher::instance())
        QnDesktopCameraResourceSearcher::instance()->registerCamera(d->socket, userName);
    d->socket.clear();
}

#endif //ENABLE_DESKTOP_CAMERA
