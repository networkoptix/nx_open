/**********************************************************
* 18 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_HTTP_SERVER_MANAGERS_H
#define NX_HTTP_SERVER_MANAGERS_H

#include "http_message_dispatcher.h"

#include <utils/common/singleton.h>


namespace nx_http
{
    class ServerManagers
    :
        public Singleton<ServerManagers>
    {
    public:
        ServerManagers();

        void setDispatcher( MessageDispatcher* const dispatcher );
        MessageDispatcher* dispatcher();

    private:
        MessageDispatcher* m_dispatcher;
    };
}

#endif  //NX_HTTP_SERVER_MANAGERS_H
