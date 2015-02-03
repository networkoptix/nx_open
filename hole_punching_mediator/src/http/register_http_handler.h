/**********************************************************
* 5 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_REGISTER_HTTP_HANDLER_H
#define NX_REGISTER_HTTP_HANDLER_H

#include <memory>

#include <QString>

#include <utils/network/http/server/http_server_connection.h>

#include "db/registered_systems_data_manager.h"


class RegisterSystemHttpHandler
{
public:
    static const QString HANDLER_PATH;

    bool processRequest( HttpServerConnection* connection, nx_http::Message&& message );
};

#endif  //NX_REGISTER_HTTP_HANDLER_H
