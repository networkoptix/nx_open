#include "auto_request_forwarder.h"

#include <QtCore/QUrlQuery>

#include <common/common_module.h>
#include <api/helpers/camera_id_helper.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/string.h>
#include <nx/utils/match/wildcard.h>
#include <utils/fs/file.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/streaming/rtsp_client.h>

#include <streaming/streaming_params.h>
#include <network/universal_request_processor.h>
#include <common/common_module.h>

#include <ini.h>

static const qint64 kUsPerMs = 1000;

QnAutoRequestForwarder::QnAutoRequestForwarder(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    if (ini().verboseAutoRequestForwarder)
    {
        const auto logger = nx::utils::log::addLogger({nx::utils::log::Tag(this)});
        logger->setDefaultLevel(nx::utils::log::Level::verbose);
        NX_VERBOSE(this) << lm("Verbose logging started: .ini verboseAutoRequestForwarder=true");
    }
}

void QnAutoRequestForwarder::processRequest(nx_http::Request* const request)
{
    // Help decluttering logs of /api/moduleInformation when debugging this class.
    if (ini().ignoreApiModuleInformationInAutoRequestForwarder
        && request->requestLine.url.path().startsWith("/api/moduleInformation"))
    {
        return;
    }

    if (isPathIgnored(request))
        return;

    const QUrlQuery urlQuery(request->requestLine.url.query());
    if (urlQuery.hasQueryItem(Qn::SERVER_GUID_HEADER_NAME)
        || request->headers.find(Qn::SERVER_GUID_HEADER_NAME) != request->headers.end())
    {
        NX_VERBOSE(this) << lm("Request [%1] is skipped: Header or param [%2] found")
            .args(request->requestLine, Qn::SERVER_GUID_HEADER_NAME);
        return;
    }
    if (urlQuery.hasQueryItem(Qn::CAMERA_GUID_HEADER_NAME)
        || request->headers.find(Qn::CAMERA_GUID_HEADER_NAME) != request->headers.end())
    {
        NX_VERBOSE(this) << lm("Request [%1] is skipped: Header or param [%2] found")
            .args(request->requestLine, Qn::CAMERA_GUID_HEADER_NAME);
        return;
    }

    if (QnUniversalRequestProcessor::isCloudRequest(*request))
    {
        auto servers = resourcePool()->getResources<QnMediaServerResource>().filtered(
            [](const QnMediaServerResourcePtr server)
            {
                return server->getServerFlags().testFlag(Qn::SF_HasPublicIP)
                    && server->getStatus() == Qn::Online;
            });
        if (!servers.isEmpty())
        {
            if (addProxyToRequest(request, servers.front()))
            {
                NX_VERBOSE(this) << lm("Cloud request [%1]: Forwarding to server %2")
                    .args(request->requestLine, servers.front()->getId());
            }
        }
        else
        {
            NX_VERBOSE(this) << lm("Cloud request [%1] is skipped: No servers found")
                .arg(request->requestLine);
        }
        return;
    }

    const bool liveStreamRequested = urlQuery.hasQueryItem(StreamingParams::LIVE_PARAM_NAME);

    QnResourcePtr camera;
    if (!findCamera(*request, urlQuery, &camera))
        return;
    NX_CRITICAL(camera);

    // Check for the time requested to select the desired server.
    qint64 timestampMs = -1;
    QnMediaServerResourcePtr server =
        camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!liveStreamRequested)
    {
        timestampMs = fetchTimestamp(*request, urlQuery);
        if (timestampMs != -1)
        {
            // Search the server for the timestamp.
            QnVirtualCameraResourcePtr virtualCamera =
                camera.dynamicCast<QnVirtualCameraResource>();
            if (virtualCamera)
            {
                QnMediaServerResourcePtr mediaServer =
                    commonModule()->cameraHistoryPool()->getMediaServerOnTimeSync(
                        virtualCamera, timestampMs);
                if (mediaServer)
                    server = mediaServer;
            }
        }
    }
    if (addProxyToRequest(request, server))
    {
        NX_VERBOSE(this) << lm("Forwarding request [%1] to server %4 (camera %2, timestamp %3)").args(
            request->requestLine,
            camera->getId().toString(),
            (timestampMs == -1)
                ? QString::fromLatin1("live")
                : QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::ISODate),
            server->getId().toString());
    }
}

bool QnAutoRequestForwarder::addProxyToRequest(
    nx_http::Request* const request,
    const QnMediaServerResourcePtr& server)
{
    if (!server)
    {
        NX_VERBOSE(this) << lm("Request [%1] is skipped: Server not found").arg(request->requestLine);
        return false;
    }

    if (server->getId() == commonModule()->moduleGUID())
    {
        NX_VERBOSE(this) << lm("Request [%1] is skipped: Target server is the current one")
            .arg(request->requestLine);
        return false;
    }

    request->headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    return true;
}

void QnAutoRequestForwarder::addPathToIgnore(const QString& pathWildcardMask)
{
    m_ignoredPathWildcardMarks.append(pathWildcardMask);
    NX_VERBOSE(this) << lm("Added path wildcard mask to ignore: [%1]").arg(pathWildcardMask);
}

bool QnAutoRequestForwarder::isPathIgnored(const nx_http::Request* request)
{
    for (const auto& mask: m_ignoredPathWildcardMarks)
    {
        QString path = request->requestLine.url.path();

        // Leave no more than one slash at the beginning.
        while (path.startsWith(lit("//")))
            path = path.mid(1);

        // Remove trailing slashes.
        while (path.endsWith(L'/'))
            path.chop(1);

        if (wildcardMatch("*" + mask, path))
        {
            NX_VERBOSE(this) << lm("Skipped: Path [%1] matches wildcard mask [%2]").args(path, mask);
            return true;
        }
    }
    return false;
}

void QnAutoRequestForwarder::addAllowedProtocolAndPathPart(
    const QByteArray& protocol, const QString& pathPart)
{
    NX_ASSERT(!protocol.isEmpty());
    NX_ASSERT(!pathPart.isEmpty());
    NX_ASSERT(!pathPart.contains('/'));

    const auto& scheme = protocol.toLower();

    m_allowedPathPartByScheme.insertMulti(scheme, pathPart);
    NX_VERBOSE(this) << lm("Added allowed path part [%1] for protocol [%2]").args(pathPart, protocol);
}

void QnAutoRequestForwarder::addCameraIdUrlParams(
    const QString& path, const QStringList& cameraIdUrlParams)
{
    NX_ASSERT(!path.isEmpty());
    NX_ASSERT(!cameraIdUrlParams.isEmpty());
    NX_ASSERT(!m_cameraIdUrlParamsByPath.contains(path));

    m_cameraIdUrlParamsByPath.insert(path, cameraIdUrlParams);
    NX_VERBOSE(this) << lm("Added camera id url params [%1] for path [%2]")
        .args(cameraIdUrlParams, path);
}

bool QnAutoRequestForwarder::findCamera(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery,
    QnResourcePtr* const outCamera)
{
    QnUuid cameraId = findCameraIdAsCameraGuidHeader(request, urlQuery);
    if (!cameraId.isNull())
    {
        *outCamera = resourcePool()->getResourceById(cameraId);
        return *outCamera != nullptr;
    }

    if (findCameraInUrlPath(request, urlQuery, outCamera))
        return true;

    if (findCameraInUrlQuery(request, urlQuery, outCamera))
        return true;

    NX_VERBOSE(this) << lm("Skipped: Camera not found");
    return false;
}

QnUuid QnAutoRequestForwarder::findCameraIdAsCameraGuidHeader(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery)
{
    QnUuid cameraId;

    // TODO: Never succeeds: presence of this header has already triggered an early exit.
    auto it = request.headers.find(Qn::CAMERA_GUID_HEADER_NAME);
    if (it != request.headers.end())
    {
        cameraId = QnUuid::fromStringSafe(it->second);
        NX_VERBOSE(this) << lm("Found camera id %1 in header [%2]")
            .args(cameraId, Qn::CAMERA_GUID_HEADER_NAME);
        return cameraId;
    }

    cameraId = QnUuid::fromStringSafe(request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME));
    if (!cameraId.isNull())
    {
        NX_VERBOSE(this) << lm("Found camera id %1 in cookie [%2]")
            .args(cameraId, Qn::CAMERA_GUID_HEADER_NAME);
        return cameraId;
    }

    // TODO: Never succeeds: presence of this query item has already triggered an early exit.
    cameraId = QnUuid::fromStringSafe(urlQuery.queryItemValue(Qn::CAMERA_GUID_HEADER_NAME));
    if (!cameraId.isNull())
    {
        NX_VERBOSE(this) << lm("Found camera id %1 in url param [%2]")
            .args(cameraId, Qn::CAMERA_GUID_HEADER_NAME);
        return cameraId;
    }

    return cameraId;
}

bool QnAutoRequestForwarder::findCameraInUrlPath(
    const nx_http::Request& request,
    const QUrlQuery& /*urlQuery*/,
    QnResourcePtr* const outCamera)
{
    const QString path = request.requestLine.url.path();
    const int lastSlashPos = path.lastIndexOf('/');

    const QString scheme = request.requestLine.version.protocol.toLower();
    bool found = false;
    for (const auto& pathPart: m_allowedPathPartByScheme.values(scheme))
    {
        if (pathPart != lit("*"))
        {
            if (lastSlashPos == -1) //< No slash in path, and pathPart is not "*".
                continue;

            // Check that pathPart precedes the last slash.
            if (path.mid(lastSlashPos - pathPart.size(), pathPart.size()) != pathPart)
                continue;
        }

        found = true;
        break;
    }
    if (!found)
        return false;

    // Get the trailing after the last '/' (if any), which has the format: <cameraId>[.<ext>]
    const QString trailing = path.mid(lastSlashPos + 1); //< Also works for lastSlashPos == -1.

    // Get the trailing part before .<ext> (if any).
    const int lastPeriodPos = trailing.lastIndexOf('.');
    const QString cameraId = trailing.left(lastPeriodPos);
    if (cameraId.isEmpty())
        return false;

    NX_VERBOSE(this) << lm("Found camera id %1 in url path").arg(cameraId);
    *outCamera = nx::camera_id_helper::findCameraByFlexibleId(resourcePool(), cameraId);
    return *outCamera != nullptr;
}

bool QnAutoRequestForwarder::findCameraInUrlQuery(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery,
    QnResourcePtr* const outCamera)
{
    const QString path = request.requestLine.url.path();

    // Path may contain an arbitrary prefix before the checked part.
    QStringList paramNames;
    for (const QString& expectedPath: m_cameraIdUrlParamsByPath.keys())
    {
        if (path.endsWith(expectedPath))
        {
            paramNames = m_cameraIdUrlParamsByPath.value(expectedPath);
            NX_ASSERT(!paramNames.isEmpty());
            break;
        }
    }
    if (paramNames.isEmpty()) //< Path trailing not found.
        return false;

    NX_VERBOSE(this) << lm("Lookingh for camera id in url params [%2]").args(paramNames);

    const QnRequestParams params = requestParamsFromUrl(request.requestLine.url);
    QString notFoundCameraId;
    *outCamera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool(), &notFoundCameraId, params, paramNames);
    if (!*outCamera && !notFoundCameraId.isNull())
        NX_VERBOSE(this) << lm("Camera not found by flexible id [%1]").args(notFoundCameraId);
    return *outCamera != nullptr;
}

qint64 QnAutoRequestForwarder::fetchTimestamp(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery)
{
    if (urlQuery.hasQueryItem(lit("time"))) // In /api/image.
    {
        const auto timeStr = urlQuery.queryItemValue(lit("time"));
        if (timeStr.toLower().trimmed() == "latest")
            return -1;
        else
            return nx::utils::parseDateTime(timeStr.toLatin1()) / kUsPerMs;
    }

    if (urlQuery.hasQueryItem(StreamingParams::START_POS_PARAM_NAME))
    {
        // In rtsp, "pos" is usec-only, no date. In http, "pos" is in millis or is the iso date.
        const auto posStr = urlQuery.queryItemValue(StreamingParams::START_POS_PARAM_NAME);
        const auto ts = nx::utils::parseDateTime(posStr);
        if (ts == DATETIME_NOW)
            return -1;
        return ts / kUsPerMs;
    }

    if (urlQuery.hasQueryItem(StreamingParams::START_TIMESTAMP_PARAM_NAME))
    {
        const auto tsStr = urlQuery.queryItemValue(StreamingParams::START_TIMESTAMP_PARAM_NAME);
        return tsStr.toLongLong() / kUsPerMs;
    }

    if (request.requestLine.version == nx_rtsp::rtsp_1_0)
    {
        // Search for position in the headers.
        auto rangeIter = request.headers.find(nx_rtsp::header::Range::NAME);
        if (rangeIter != request.headers.end())
        {
            qint64 startTimestamp = 0;
            qint64 endTimestamp = 0;
            if (nx_rtsp::parseRangeHeader(rangeIter->second, &startTimestamp, &endTimestamp))
                return startTimestamp / kUsPerMs;
        }
    }

    return -1;
}
