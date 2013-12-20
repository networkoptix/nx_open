/**********************************************************
* 18 sep 2013
* a.kolesnikov
***********************************************************/

#if 0

#ifndef HOLE_PUNCHING_REQUESTS_PROCESSOR_H
#define HOLE_PUNCHING_REQUESTS_PROCESSOR_H

#include <utils/network/abstract_socket.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/http/abstract_http_connection.h>


//!Connection from host, trying to establish connection from behind NAT to another host hidden behind another NAT
class HolePunchingClientConnection
:
    public nx_http::AbstractHttpConnection
{
public:
    //!Initialization
    HolePunchingClientConnection();

    //!Implementation of nx_http::AbstractHttpConnection::processRequest
    virtual void processRequest(
        const nx_http::HttpRequest& request,
        nx_http::HttpResponse* const response ) override;
    //!Implementation of nx_http::AbstractHttpConnection::onConnectionClosed
    virtual void onConnectionClosed() override;

private:
    void processBindRequest(
        const QString& hostID,
        const nx_http::HttpRequest& request,
        nx_http::HttpResponse* const response );
    void processConnectRequest(
        const QString& hostID,
        const nx_http::HttpRequest& request,
        nx_http::HttpResponse* const response );
};

#endif  //HOLE_PUNCHING_REQUESTS_PROCESSOR_H

#endif
