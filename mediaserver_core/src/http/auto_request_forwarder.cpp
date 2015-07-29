/**********************************************************
* Jul 28, 2015
* a.kolesnikov
***********************************************************/

#include "auto_request_forwarder.h"

#include <QtCore/QUrlQuery>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <http/custom_headers.h>
#include <utils/fs/file.h>
#include <utils/network/rtsp/rtsp_types.h>


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

        //TODO checking for the time requested to select desired server

        auto parentRes = cameraRes->getParentResource();
        if( dynamic_cast<QnMediaServerResource*>(parentRes.data()) == nullptr )
            return;
        request->headers.emplace(
            Qn::SERVER_GUID_HEADER_NAME,
            parentRes->getId().toByteArray() );
    }
}

bool QnAutoRequestForwarder::findCameraGuid(
    const nx_http::Request& request,
    const QUrlQuery urlQuery,
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
    const QUrlQuery urlQuery,
    QnResourcePtr* const res )
{
    return findCameraUniqueIDInPath( request, res ) ||
           findCameraUniqueIDInQuery( urlQuery, res );
}

bool QnAutoRequestForwarder::findCameraUniqueIDInPath(
    const nx_http::Request& request,
    QnResourcePtr* const res )
{
    if( request.requestLine.version == nx_rtsp::rtsp_1_0 )
        int x = 0;

    //path containing camera unique_id looks like: /[something/]unique_id[.extension]

    const QString& requestedResourcePath = QnFile::fileName( request.requestLine.url.path() );
    const int nameFormatSepPos = requestedResourcePath.lastIndexOf( QLatin1Char( '.' ) );
    const QString& resUniqueID = requestedResourcePath.mid( 0, nameFormatSepPos );

    if( resUniqueID.isEmpty() )
        return false;

    *res = qnResPool->getResourceByUniqueId( resUniqueID );
    return *res;
}

bool QnAutoRequestForwarder::findCameraUniqueIDInQuery(
    const QUrlQuery urlQuery,
    QnResourcePtr* const res )
{
    const auto uniqueID = urlQuery.queryItemValue( Qn::CAMERA_UNIQUE_ID_HEADER_NAME );
    if( uniqueID.isEmpty() )
        return false;
    *res = qnResPool->getResourceByUniqueId( uniqueID );
    return *res;
}
