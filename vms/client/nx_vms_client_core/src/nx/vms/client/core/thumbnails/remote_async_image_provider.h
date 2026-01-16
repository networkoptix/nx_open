// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::core {

/**
 * This asynchronous QML image provider performs an HTTP request through the server connection and
 * expects a binary image (image/png, image/jpg or image/tiff) or JSON error in response.
 * It wraps GenericRemoteImageRequest, and adds caching functionality.
 *
 * The currently connected server connection is used, unless the internal parameter "___systemId"
 * (the name can be queried via `systemIdParameterName() method) is present in the request line;
 * in that case the corresponding cross system connection is used.
 *
 * If an image in the cache should expire after a timeout, the internal parameter
 * "___expirationSeconds" (the name can be queried via `expirationParameterName()` method)
 * with a desired expiration interval in *seconds* should be added to the request line.
 */
class NX_VMS_CLIENT_CORE_API RemoteAsyncImageProvider: public QQuickAsyncImageProvider
{
public:
    RemoteAsyncImageProvider();

    virtual QQuickImageResponse* requestImageResponse(
        const QString& requestLine, const QSize& /*requestedSize*/) override;

    static QString systemIdParameterName();
    static QString expirationParameterName();

private:
    class Response;
    class RequestCache;
    const std::shared_ptr<RequestCache> m_cache;
};

} // namespace nx::vms::client::core
