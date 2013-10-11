/**********************************************************
* 19 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_HTTP_CONNECTION_H
#define ABSTRACT_HTTP_CONNECTION_H

#include "httptypes.h"
#include "../abstract_socket.h"
#include "../../common/stoppable.h"


namespace nx_http
{
    class QN_EXPORT AbstractHttpRequestProcessor
    {
    public:
        virtual ~AbstractHttpRequestProcessor() {}

        //!Process received request without message body (it is delivered by virtual call \a AbstractHttpConnection::gotSomeMessageBody)
        /*! 
            \paam response This response will be sent in asyncronous mode after return of this method
        */
        virtual void processRequest(
            const HttpRequest& request,
            HttpResponse* const response ) = 0;
        //!Request message body is passed to this method
        /*!
            \param eof true, if end of message body has been reached
        */
        virtual void gotSomeMessageBody( const BufferType& msgBody, bool eof ) = 0;
    };

    //!Server-side http connection
    /*!
        \warning Implementation uses sockets in asynchronous mode only. So!!! Any overloaded method MUST NOT BLOCK!!!
        \todo Clarify instance life-time
    */
    class QN_EXPORT AbstractHttpConnection
    :
        public QnStoppable,
        public AbstractHttpRequestProcessor
    {
    public:
        virtual ~AbstractHttpConnection() {}

        //!Implementation of QnStoppable::pleaseStop()
        virtual void pleaseStop() override;

        //!Returns socket
        const AbstractStreamSocket* socket() const;
        AbstractStreamSocket* socket();

        //!Called after connection has been closed and \a AbstractHttpConnection object is about to be destroyed
        virtual void onConnectionClosed() {};
    };
}

#endif //ABSTRACT_HTTP_CONNECTION_H
