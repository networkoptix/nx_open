/**********************************************************
* 5 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_REGISTER_HTTP_HANDLER_H
#define NX_REGISTER_HTTP_HANDLER_H

#include <memory>

#include <QString>

#include "db/registered_systems_data_manager.h"
#include "http_server_connection.h"


class RegisterSystemHttpHandler
{
public:
    static const QString HANDLER_PATH;

    bool processRequest( const std::weak_ptr<HttpServerConnection>& connection, nx_http::Message&& message );

private:
    void responseSent( const std::weak_ptr<HttpServerConnection>& connection );
};

#endif  //NX_REGISTER_HTTP_HANDLER_H
