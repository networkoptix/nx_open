/**********************************************************
* 1 jun 2015
* a.kolesnikov
***********************************************************/

#ifndef nx_http_abstract_authentication_manager_h
#define nx_http_abstract_authentication_manager_h

#include "http_server_connection.h"


namespace nx_http
{
    class AbstractAuthenticationManager
    {
    public:
        virtual ~AbstractAuthenticationManager() {}

        /*!
            \return \a false if could not authenticate \a request
        */
        virtual bool authenticate(
            const HttpServerConnection& connection,
            const nx_http::Request& request,
            header::WWWAuthenticate* const wwwAuthenticate ) = 0;
    };
}

#endif  //nx_http_abstract_authentication_manager_h
