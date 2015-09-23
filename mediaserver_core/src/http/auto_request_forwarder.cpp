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
#include <http/custom_headers.h>
#include <utils/common/string.h>
#include <utils/fs/file.h>
#include <utils/network/rtsp/rtsp_types.h>
#include <utils/network/rtpsession.h>

#include "streaming/streaming_params.h"


static const qint64 USEC_PER_MS = 1000;

void QnAutoRequestForwarder::processRequest( nx_http::Request* const request )
{
    const QUrlQuery urlQuery( request->requestLine.url.query() );

    if( urlQuery.hasQueryItem( Qn::SERVER_GUID_HEADER_NAME ) ||
        request->headers.find( Qn::SERVER_GUID_HEADER_NAME ) != request->headers.end() )
    {
        //SERVER_GUID_HEADER_NAME already present
        return;
    }

    QnResourcePtr cameraRes;
    if( findCameraGuid( *request, urlQuery, &cameraRes ) ||
        findCameraUniqueID( *request, urlQuery, &cameraRes ) )
    {
        //detecting owner of res and adding SERVER_GUID_HEADER_NAME
        Q_ASSERT( cameraRes );

        //checking for the time requested to select desired server
        const qint64 timestampMs = fetchTimestamp( *request, urlQuery );

        QnMediaServerResourcePtr serverRes =
            cameraRes->getParentResource().dynamicCast<QnMediaServerResource>();
        if( timestampMs != -1 )
        {
            QnVirtualCameraResourcePtr virtualCameraRes = cameraRes.dynamicCast<QnVirtualCameraResource>();
            if( virtualCameraRes )
            {
                QnMediaServerResourcePtr mediaServer =
                    QnCameraHistoryPool::instance()->getMediaServerOnTime( virtualCameraRes, timestampMs );
                if( mediaServer )
                    serverRes = mediaServer;
            }
        }
        if( !serverRes )
            return; //no current server?
        if( serverRes->getId() == qnCommon->moduleGUID() )
            return; //target server is this one
        request->headers.emplace(
            Qn::SERVER_GUID_HEADER_NAME,
            serverRes->getId().toByteArray() );
    }
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
        cameraGuid = xCameraGuidIter->second;

    if( cameraGuid.isNull() )
        cameraGuid = request.getCookieValue( Qn::CAMERA_GUID_HEADER_NAME );

    if( cameraGuid.isNull() )
        cameraGuid = urlQuery.queryItemValue( Qn::CAMERA_GUID_HEADER_NAME );

    if( cameraGuid.isNull() )
        return false;

    *res = qnResPool->getResourceById( cameraGuid );
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
    *res = qnResPool->getResourceByUniqueId(resUniqueID);
    if( *res )
        return true;
    //searching by mac
    //*res = qnResPool->getResourceByMacAddress(resUniqueID);
    return *res != nullptr;
}

bool QnAutoRequestForwarder::findCameraUniqueIDInQuery(
    const QUrlQuery& urlQuery,
    QnResourcePtr* const res )
{
    const auto uniqueID = urlQuery.queryItemValue( Qn::CAMERA_UNIQUE_ID_HEADER_NAME );
    if( uniqueID.isEmpty() )
        return false;
    *res = qnResPool->getResourceByUniqueId( uniqueID );
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
            return parseDateTime( timeStr.toLatin1() ) / USEC_PER_MS;
    }

    if( urlQuery.hasQueryItem( StreamingParams::START_POS_PARAM_NAME ) )
    {
        //in rtsp "pos" is usec only, no date! In http "pos" is in millis or iso date
        const auto posStr = urlQuery.queryItemValue( StreamingParams::START_POS_PARAM_NAME );
        const auto ts = parseDateTime( posStr );
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
