/**********************************************************
* Jul 28, 2015
* a.kolesnikov
***********************************************************/

#include "auto_request_forwarder.h"

#include <QtCore/QUrlQuery>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <utils/fs/file.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/streaming/rtsp_client.h>

#include "streaming/streaming_params.h"
#include <network/universal_request_processor.h>
#include <common/common_module.h>

static const qint64 USEC_PER_MS = 1000;

QnAutoRequestForwarder::QnAutoRequestForwarder(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void QnAutoRequestForwarder::processRequest( nx_http::Request* const request )
{
    const auto allowedMethods = m_restrictionList.getAllowedAuthMethods(*request);
    //TODO #ak nx_http::AuthMethod::videowall is used here to imply existing class
        //nx_http::AuthMethodRestrictionList with no change, since release 2.5 is coming.
        //Proper types will be introduced in 2.6
    if (!(allowedMethods & nx_http::AuthMethod::videowall))
        return; //not processing url

    const QUrlQuery urlQuery( request->requestLine.url.query() );

    if( urlQuery.hasQueryItem( Qn::SERVER_GUID_HEADER_NAME ) ||
        request->headers.find( Qn::SERVER_GUID_HEADER_NAME ) != request->headers.end() )
    {
        //SERVER_GUID_HEADER_NAME already present
        return;
    }

    if( urlQuery.hasQueryItem( Qn::CAMERA_GUID_HEADER_NAME ) ||
        request->headers.find( Qn::CAMERA_GUID_HEADER_NAME ) != request->headers.end() )
    {
        //CAMERA_GUID_HEADER_NAME already present
        return;
    }

    if (QnUniversalRequestProcessor::isCloudRequest(*request))
    {
        auto servers = resourcePool()->getResources<QnMediaServerResource>().filtered(
            [](const QnMediaServerResourcePtr server)
            {
                return server->getServerFlags().testFlag(Qn::SF_HasPublicIP) &&
                       server->getStatus() == Qn::Online;
            });
        if (!servers.isEmpty())
        {
            if (addProxyToRequest(request, servers.front()))
            {
                NX_LOG(lit("auto_forward. Forwarding request %1 to server %2").
                    arg(request->requestLine.url.path()).
                    arg(servers.front()->getId().toString()), cl_logDEBUG2);
            }
        }
        return;
    }

    const bool liveStreamRequested = urlQuery.hasQueryItem( StreamingParams::LIVE_PARAM_NAME );

    QnResourcePtr cameraRes;
    if( findCameraGuid( *request, urlQuery, &cameraRes ) ||
        findCameraUniqueID( *request, urlQuery, &cameraRes ) )
    {
        //detecting owner of res and adding SERVER_GUID_HEADER_NAME
        NX_ASSERT( cameraRes );

        //checking for the time requested to select desired server
        qint64 timestampMs = -1;

        QnMediaServerResourcePtr serverRes =
            cameraRes->getParentResource().dynamicCast<QnMediaServerResource>();
        if( !liveStreamRequested )
        {
            timestampMs = fetchTimestamp( *request, urlQuery );
            if( timestampMs != -1 )
            {
                //searching server for timestamp
                QnVirtualCameraResourcePtr virtualCameraRes = cameraRes.dynamicCast<QnVirtualCameraResource>();
                if( virtualCameraRes )
                {
                    QnMediaServerResourcePtr mediaServer =
                        commonModule()->cameraHistoryPool()->getMediaServerOnTimeSync( virtualCameraRes, timestampMs );
                    if( mediaServer )
                        serverRes = mediaServer;
                }
            }
        }
        if (addProxyToRequest(request, serverRes))
        {
            NX_LOG(lit("auto_forward. Forwarding request %1 (resource %2, timestamp %3) to server %4").
                arg(request->requestLine.url.path()).arg(cameraRes->getId().toString()).
                arg(timestampMs == -1 ? QString::fromLatin1("live") : QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::ISODate)).
                arg(serverRes->getId().toString()), cl_logDEBUG2);
        }
    }
}

bool QnAutoRequestForwarder::addProxyToRequest(
    nx_http::Request* const request,
    const QnMediaServerResourcePtr& serverRes)
{
    if (!serverRes)
        return false;
    if (serverRes->getId() == commonModule()->moduleGUID())
        return false; //target server is this one
    request->headers.emplace(
        Qn::SERVER_GUID_HEADER_NAME,
        serverRes->getId().toByteArray());
    return true;
}

void QnAutoRequestForwarder::addPathToIgnore(const QString& pathWildcardMask)
{
    m_restrictionList.deny(pathWildcardMask, nx_http::AuthMethod::videowall);
}

bool QnAutoRequestForwarder::findCameraGuid(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery,
    QnResourcePtr* const res )
{
    QnUuid cameraGuid;

    nx_http::HttpHeaders::const_iterator xCameraGuidIter =
        request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
    if( xCameraGuidIter != request.headers.end() )
        cameraGuid = QnUuid::fromStringSafe(xCameraGuidIter->second);

    if( cameraGuid.isNull() )
        cameraGuid = QnUuid::fromStringSafe(request.getCookieValue( Qn::CAMERA_GUID_HEADER_NAME ));

    if( cameraGuid.isNull() )
        cameraGuid = QnUuid::fromStringSafe(urlQuery.queryItemValue( Qn::CAMERA_GUID_HEADER_NAME ));

    if( cameraGuid.isNull() )
        return false;

    *res = resourcePool()->getResourceById( cameraGuid );
    return *res;
}

bool QnAutoRequestForwarder::findCameraUniqueID(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery,
    QnResourcePtr* const res )
{
    return findCameraUniqueIDInPath( request, res ) ||
           findCameraUniqueIDInQuery( urlQuery, res );
}

bool QnAutoRequestForwarder::findCameraUniqueIDInPath(
    const nx_http::Request& request,
    QnResourcePtr* const res )
{
    //path containing camera unique_id looks like: /[something/]unique_id[.extension]

    const QString& requestedResourcePath = QnFile::fileName( request.requestLine.url.path() );
    const int nameFormatSepPos = requestedResourcePath.lastIndexOf( QLatin1Char( '.' ) );
    const QString& resUniqueID = requestedResourcePath.mid( 0, nameFormatSepPos );

    if( resUniqueID.isEmpty() )
        return false;

    //resUniqueID could be physical id or mac address
    //trying luck with physical id
    *res = resourcePool()->getResourceByUniqueId(resUniqueID);
    if( *res )
    {
        NX_LOG(lit("auto_forward. Found resource %1 by unique id %2 from path").
            arg((*res)->getId().toString()).arg(resUniqueID), cl_logDEBUG2);
        return true;
    }
    //searching by mac
    //*res = resourcePool()->getResourceByMacAddress(resUniqueID);
    return *res != nullptr;
}

bool QnAutoRequestForwarder::findCameraUniqueIDInQuery(
    const QUrlQuery& urlQuery,
    QnResourcePtr* const res )
{
    const auto uniqueID = urlQuery.queryItemValue( Qn::CAMERA_UNIQUE_ID_HEADER_NAME );
    if( uniqueID.isEmpty() )
        return false;
    *res = resourcePool()->getResourceByUniqueId( uniqueID );
    return *res;
}

qint64 QnAutoRequestForwarder::fetchTimestamp(
    const nx_http::Request& request,
    const QUrlQuery& urlQuery )
{
    if( urlQuery.hasQueryItem(lit("time")) )    //in /api/image
    {
        const auto timeStr = urlQuery.queryItemValue( lit("time") );
        if( timeStr.toLower().trimmed() == "latest" )
            return -1;
        else
            return nx::utils::parseDateTime( timeStr.toLatin1() ) / USEC_PER_MS;
    }

    if( urlQuery.hasQueryItem( StreamingParams::START_POS_PARAM_NAME ) )
    {
        //in rtsp "pos" is usec only, no date! In http "pos" is in millis or iso date
        const auto posStr = urlQuery.queryItemValue( StreamingParams::START_POS_PARAM_NAME );
        const auto ts = nx::utils::parseDateTime( posStr );
        if( ts == DATETIME_NOW )
            return -1;
        return ts / USEC_PER_MS;
    }

    if( urlQuery.hasQueryItem( StreamingParams::START_TIMESTAMP_PARAM_NAME ) )
    {
        const auto tsStr = urlQuery.queryItemValue( StreamingParams::START_TIMESTAMP_PARAM_NAME );
        return tsStr.toLongLong() / USEC_PER_MS;
    }

    if( request.requestLine.version == nx_rtsp::rtsp_1_0 )
    {
        //searching for position in headers
        auto rangeIter = request.headers.find( nx_rtsp::header::Range::NAME );
        if( rangeIter != request.headers.end() )
        {
            qint64 startTimestamp = 0;
            qint64 endTimestamp = 0;
            if( nx_rtsp::parseRangeHeader( rangeIter->second, &startTimestamp, &endTimestamp ) )
                return startTimestamp / USEC_PER_MS;
        }
    }

    return -1;
}
