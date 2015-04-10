/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#if 0

#include "hole_punching_requests_processor.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include "version.h"


//!With this request host informs, that it accepting connections. Example: /bind?host_id=host_sweet_host
static const QString registerHostPath = "/bind";
//!By this request host connects to another host accepting connections. Example: /connect?host_id=host_sweet_host
static const QString connectToHostPath = "/connect";
static const QString hostIDParamName = "host_id";

HolePunchingClientConnection::HolePunchingClientConnection()
{
}

void HolePunchingClientConnection::processRequest(
    const nx_http::HttpRequest& request,
    nx_http::HttpResponse* const response )
{
    QUrlQuery urlQuery( request.requestLine.url.query() );
    const QString& hostID = urlQuery.queryItemValue( hostIDParamName );

    response->headers.insert( nx_http::HttpHeader("Server", QN_APPLICATION_NAME" v."QN_APPLICATION_VERSION) );

    if( request.requestLine.url.path() == registerHostPath )
        processBindRequest( hostID, request, response );
    else if( request.requestLine.url.path() == connectToHostPath )
        processConnectRequest( hostID, request, response );

    //TODO/IMPL
}

void HolePunchingClientConnection::onConnectionClosed()
{
    //TODO/IMPL
}

void HolePunchingClientConnection::processBindRequest(
    const QString& hostID,
    const nx_http::HttpRequest& request,
    nx_http::HttpResponse* const response )
{
    //TODO/IMPL 
        //registering remote host, checking host_id uniqueness
}

void HolePunchingClientConnection::processConnectRequest(
    const QString& hostID,
    const nx_http::HttpRequest& request,
    nx_http::HttpResponse* const response )
{
    //TODO/IMPL
        //searching requested host
        //if none, - 404


}

#endif
