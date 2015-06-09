/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "http_transaction_receiver.h"

#include <rest/server/rest_connection_processor.h>
#include <utils/network/tcp_connection_priv.h>

#include "http/custom_headers.h"
#include "transaction/transaction_message_bus.h"
#include "transaction/transaction_transport.h"


namespace ec2
{
    ////////////////////////////////////////////////////////////
    //// class QnRestTransactionReceiver
    ////////////////////////////////////////////////////////////

    QnRestTransactionReceiver::QnRestTransactionReceiver()
    {
    }

    int QnRestTransactionReceiver::executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray&, /*contentType*/ 
        const QnRestConnectionProcessor* )
    {
        return nx_http::StatusCode::badRequest;
    }

    int QnRestTransactionReceiver::executePost(
        const QString& path,
        const QnRequestParamList& /*params*/,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& resultBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* connection )
    {
        auto connectionGuidIter = connection->request().headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
        if( connectionGuidIter == connection->request().headers.end() )
            return nx_http::StatusCode::forbidden;
        const QnUuid connectionGuid( connectionGuidIter->second );

        if( !QnTransactionMessageBus::instance()->gotTransactionFromRemotePeer(
                connectionGuid,
                connection->request(),
                body ) )
        {
            return nx_http::StatusCode::notFound;
        }

        return nx_http::StatusCode::ok;
    }



    ////////////////////////////////////////////////////////////
    //// class QnHttpTransactionReceiver
    ////////////////////////////////////////////////////////////

    class QnHttpTransactionReceiverPrivate
    :
        public QnTCPConnectionProcessorPrivate
    {
    public:

        QnHttpTransactionReceiverPrivate()
        : 
            QnTCPConnectionProcessorPrivate()
        {
        }
    };

    QnHttpTransactionReceiver::QnHttpTransactionReceiver(
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* _owner )
    :
        QnTCPConnectionProcessor( new QnHttpTransactionReceiverPrivate, socket )
    {
        Q_UNUSED(_owner)

        setObjectName( "QnHttpTransactionReceiver" );
    }

    QnHttpTransactionReceiver::~QnHttpTransactionReceiver()
    {
        stop();
    }

    void QnHttpTransactionReceiver::run()
    {
        Q_D(QnHttpTransactionReceiver);
        initSystemThreadId();

        if( !d->socket->setRecvTimeout(
                QnTransactionTransport::TCP_KEEPALIVE_TIMEOUT *
                QnTransactionTransport::KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE ) )
        {
            const int osErrorCode = SystemError::getLastOSErrorCode();
            NX_LOG( lit("Failed to set timeout for HTTP connection from %1. %2").
                arg(d->socket->getForeignAddress().toString()).arg(SystemError::toString(osErrorCode)), cl_logWARNING );
            return;
        }

        QnUuid connectionGuid;
        for( ;; )
        {
            if( !connectionGuid.isNull() )
            {
                //waiting for connection to be ready to receive more transactions
                QnTransactionMessageBus::instance()->waitForNewTransactionsReady( connectionGuid );
            }

            if( !readSingleRequest() )
            {
                if( d->prevSocketError == SystemError::timedOut && !connectionGuid.isNull() )
                    QnTransactionMessageBus::instance()->connectionTimedout( connectionGuid );
                return;
            }
            
            if( d->request.requestLine.method != nx_http::Method::POST )
            {
                sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
                return;
            }

            auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
            if( connectionGuidIter == d->request.headers.end() )
            {
                sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
                return;
            }

            const QnUuid requestConnectionGuid( connectionGuidIter->second );
            if( connectionGuid.isNull() )
            {
                connectionGuid = requestConnectionGuid;
            }
            else if( requestConnectionGuid != connectionGuid )
            {
                //not allowing to use TCP same connection for multiple transaction connections
                sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
                return;
            }

            if( !QnTransactionMessageBus::instance()->gotTransactionFromRemotePeer(
                    connectionGuid,
                    d->request,
                    d->requestBody ) )
            {
                NX_LOG( lit("QnHttpTransactionReceiver. Received transaction from %1 for unknown connection %2").
                    arg(d->socket->getForeignAddress().toString()).arg(connectionGuid.toString()), cl_logWARNING );
                sendResponse( nx_http::StatusCode::notFound, nx_http::StringType() );
                return;
            }

            //checking whether connection persistent or not...
            bool closeConnection = true;
            if( d->request.requestLine.version == nx_http::http_1_1 )
            {
                closeConnection = false;
                auto connectionHeaderIter = d->request.headers.find( "Connection" );
                if( connectionHeaderIter != d->request.headers.end() &&
                    connectionHeaderIter->second == "close" )
                {
                    //client requested connection closure
                    closeConnection = true;
                    d->response.headers.insert( *connectionHeaderIter );
                }
            }

            sendResponse( nx_http::StatusCode::ok, nx_http::StringType() );

            if( closeConnection )
                return;
        }
    }
}
