#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/http_types.h>
#include <common/common_module_aware.h>

/**
 * Forwards requests addressed to resources belonging to other servers.
 */
class QnAutoRequestForwarder: public QnCommonModuleAware
{
public:
    QnAutoRequestForwarder(QnCommonModule* commonModule);

    /** Analyze request and add proxy information if needed. */
    void processRequest(nx::network::http::Request* const request);

    /**
     * Ignore requests with path which ends with the specified wildcard mask, thus, may have an
     * arbitrary prefix.
     */
    void addPathToIgnore(const QString& pathWildcardMask);

    /**
     * Perform forwarding for the specified protocol (url scheme) and url path part. Camera id is
     * considered to be the last path component, and should be preceded with pathPart and a slash.
     * Hence, the URL format: <scheme>://...<pathPart>/<cameraId>[.<ext>]
     *
     * @param pathPart Part of the path after an arbitrary prefix and before camera id. Should be
     * specified without leading and trailing slashes, and should not contain slashes. If equals to
     * "*", is not checked.
     */
    void addAllowedProtocolAndPathPart(const QByteArray& protocol, const QString& pathPart);

    /**
     * Require forwarding for the specified url path, if query params contain any of the specified
     * names. The url may have an arbitrary prefix before the specified path. Hence, the URL
     * format: <scheme>://...<path>?<cameraIdParam>=<cameraId>...
     *
     * @param path Should have the format like "/api/ptz".
     */
    void addCameraIdUrlParams(const QString& path, const QStringList& cameraIdUrlParams);

private:
    QStringList m_ignoredPathWildcardMarks;
    QMultiHash<QString, QString> m_allowedPathPartByScheme;
    QHash<QString, QStringList> m_cameraIdUrlParamsByPath;

    bool isPathIgnored(const nx::network::http::Request* request);

    bool findCamera(
        const nx::network::http::Request& request,
        const QUrlQuery& urlQuery,
        QnResourcePtr* const resource);

    /** @return Null QnUuid if not found. */
    QnUuid findCameraIdAsCameraGuidHeader(
        const nx::network::http::Request& request,
        const QUrlQuery& urlQuery);

    bool findCameraInUrlPath(
        const nx::network::http::Request& request,
        const QUrlQuery& urlQuery,
        QnResourcePtr* const resource);

    bool findCameraInUrlQuery(
        const nx::network::http::Request& request,
        const QUrlQuery& urlQuery,
        QnResourcePtr* const resource);

    /** @return UTC time (milliseconds). */
    qint64 fetchTimestamp(const nx::network::http::Request& request, const QUrlQuery& urlQuery);

    /**
     * Update the request, to proxy it to the specified server.
     * @return True if the request was forwarded.
     */
    bool addProxyToRequest(
        nx::network::http::Request* const request,
        const QnMediaServerResourcePtr& serverRes);
};
