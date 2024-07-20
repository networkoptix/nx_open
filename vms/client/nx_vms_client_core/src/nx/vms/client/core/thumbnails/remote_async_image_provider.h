// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickImageProvider>

#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

/**
 * This asynchronous QML image provider performs an HTTP request through the server connection from
 * specified system context and expects a binary image (image/png, image/jpg or image/tiff) or JSON
 * error in response. It wraps GenericRemoteImageRequest.
 */
class RemoteAsyncImageProvider:
    public QQuickAsyncImageProvider,
    public SystemContextAware
{
public:
    explicit RemoteAsyncImageProvider(SystemContext* systemContext);

    virtual QQuickImageResponse* requestImageResponse(
        const QString& requestLine, const QSize& /*requestedSize*/) override;
};

} // namespace nx::vms::client::core
