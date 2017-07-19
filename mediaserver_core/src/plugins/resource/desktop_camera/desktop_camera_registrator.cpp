
#ifdef ENABLE_DESKTOP_CAMERA

#include "desktop_camera_registrator.h"
#include "desktop_camera_resource_searcher.h"
#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"
#include "nx_ec/data/api_statistics.h"

QnDesktopCameraRegistrator::QnDesktopCameraRegistrator(
    QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner)
:
    QnTCPConnectionProcessor(socket, owner)
{
    Q_D(QnTCPConnectionProcessor);
    d->isSocketTaken = true;
}

void QnDesktopCameraRegistrator::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();
    sendResponse(nx_http::StatusCode::ok, QByteArray());

    const QByteArray& userName = nx_http::getHeaderValue(d->request.headers, "user-name");
    const QByteArray userId = nx_http::getHeaderValue(d->request.headers, "user-id");

	if (auto s = QnDesktopCameraResourceSearcher::instance())
        s->registerCamera(d->socket, userName, userId);

	d->socket.clear();
}

#endif //ENABLE_DESKTOP_CAMERA
