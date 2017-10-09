#include "desktop_camera_registrator.h"

#if defined(ENABLE_DESKTOP_CAMERA)

#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>

#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>

#include <nx/utils/log/log_main.h>

QnDesktopCameraRegistrator::QnDesktopCameraRegistrator(
    QSharedPointer<AbstractStreamSocket> socket,
    QnTcpListener* owner)
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

    const auto userName = getHeaderValue(d->request.headers, "user-name");
    auto uniqueId = getHeaderValue(d->request.headers, "unique-id");
    // TODO: #GDM #3.2 Remove 3.1 compatibility layer.
    if (uniqueId.isEmpty())
        uniqueId = getHeaderValue(d->request.headers, "user-id");

    // Make sure desktop camera of another user will not substitute existing one.
    if (auto searcher = QnDesktopCameraResourceSearcher::instance())
    {
        NX_VERBOSE(this, lm("Registered desktop camera %1").arg(userName));
        searcher->registerCamera(d->socket, userName, uniqueId);
    }

    d->socket.clear();
}

#endif // defined(ENABLE_DESKTOP_CAMERA)
