// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::core {

/**
 * This asynchronous QML image provider performs an HTTP request through the server connection and
 * expects a binary image (image/png, image/jpg or image/tiff) or JSON error in response.
 * It wraps GenericRemoteImageRequest.
 *
 * The currently connected server connection is used, unless the internal parameter "___systemId"
 * is present in the request line; in that case the corresponding cross system connection is used.
 * The parameter name can be queried via `RemoteAsyncImageProvider::systemIdParameter()` method.
 */
class NX_VMS_CLIENT_CORE_API RemoteAsyncImageProvider: public QQuickAsyncImageProvider
{
public:
    virtual QQuickImageResponse* requestImageResponse(
        const QString& requestLine, const QSize& /*requestedSize*/) override;

    static QString systemIdParameter();
};

} // namespace nx::vms::client::core
