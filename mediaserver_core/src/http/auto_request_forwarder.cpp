/**********************************************************
* Jul 28, 2015
* a.kolesnikov
***********************************************************/

#include "auto_request_forwarder.h"

#include <QtCore/QUrlQuery>

#include <core/resource_management/resource_pool.h>
#include <http/custom_headers.h>


void QnAutoRequestForwarder::processRequest( nx_http::Request* const request )
{
    const QUrlQuery urlQuery( request->requestLine.url.query() );

    if( urlQuery.hasQueryItem( Qn::SERVER_GUID_HEADER_NAME ) ||
        request->headers.find( Qn::SERVER_GUID_HEADER_NAME ) != request->headers.end() )
    {
        //SERVER_GUID_HEADER_NAME already present
        return;
    }

    //TODO #ak
    //checking for CAMERA_GUID_HEADER_NAME presence
    QnResourcePtr res;
    if( findCameraGuid( *request, urlQuery, &res ) ||
        findCameraUniqueID( *request, urlQuery, &res ) )
    {
        //detecting owner of res and adding SERVER_GUID_HEADER_NAME
    }
}

bool QnAutoRequestForwarder::findCameraGuid(
    const nx_http::Request& request,
    const QUrlQuery urlQuery,
    QnResourcePtr* const res )
{
    QnUuid cameraGuid;

    nx_http::HttpHeaders::const_iterator xCameraGuidIter = request.headers.find( Qn::CAMERA_GUID_HEADER_NAME );
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
    return false;
}
