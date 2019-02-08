#include "desktop_camera_registrator.h"

#if defined(ENABLE_DESKTOP_CAMERA)

#include <plugins/resource/desktop_camera/desktop_camera_resource_searcher.h>

#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>

#include <nx/utils/log/log_main.h>
#include <media_server/media_server_module.h>
#include <media_server/media_server_resource_searchers.h>

QnDesktopCameraRegistrator::QnDesktopCameraRegistrator(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    QnTCPConnectionProcessor(std::move(socket), owner)
{
    Q_D(QnTCPConnectionProcessor);
}

void QnDesktopCameraRegistrator::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();
    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    const auto userName = getHeaderValue(d->request.headers, "user-name");
    auto uniqueId = getHeaderValue(d->request.headers, "unique-id");
    // TODO: #GDM #3.2 Remove 3.1 compatibility layer.
    if (uniqueId.isEmpty())
        uniqueId = getHeaderValue(d->request.headers, "user-id");

    // Make sure desktop camera of another user will not substitute existing one.
    auto searcher =
        serverModule()->resourceSearchers()->searcher<QnDesktopCameraResourceSearcher>();
    if (searcher)
    {
        NX_VERBOSE(this, lm("Registered desktop camera %1").arg(userName));
        searcher->registerCamera(std::move(d->socket), userName, uniqueId);
    }
}

#endif // defined(ENABLE_DESKTOP_CAMERA)
