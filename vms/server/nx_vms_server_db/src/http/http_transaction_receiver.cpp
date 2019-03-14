/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "http_transaction_receiver.h"

#include <api/global_settings.h>
#include <rest/server/rest_connection_processor.h>
#include <network/tcp_connection_priv.h>

#include <nx/network/http/custom_headers.h>
#include "transaction/server_transaction_message_bus.h"
#include "transaction/transaction_transport.h"


namespace ec2
{
    ////////////////////////////////////////////////////////////
    //// class QnRestTransactionReceiver
    ////////////////////////////////////////////////////////////

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
            QnTCPConnectionProcessorPrivate(),
            messageBus(nullptr)
        {
        }

        ServerTransactionMessageBus* messageBus;
    };

    QnHttpTransactionReceiver::QnHttpTransactionReceiver(
        ServerTransactionMessageBus* messageBus,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner )
    :
        QnTCPConnectionProcessor( new QnHttpTransactionReceiverPrivate, std::move(socket), owner)
    {
        setObjectName( "QnHttpTransactionReceiver" );
        Q_D(QnHttpTransactionReceiver);
        d->messageBus = messageBus;
    }

    QnHttpTransactionReceiver::~QnHttpTransactionReceiver()
    {
        stop();
    }

    void QnHttpTransactionReceiver::run()
    {
        Q_D(QnHttpTransactionReceiver);

        initSystemThreadId();

        const auto& globalSettings = d->messageBus->commonModule()->globalSettings();
        if( !d->socket->setRecvTimeout(
                std::chrono::milliseconds(globalSettings->connectionKeepAliveTimeout()).count() *
            globalSettings->keepAliveProbeCount()) ||
            !d->socket->setNoDelay(true) )
        {
            const int osErrorCode = SystemError::getLastOSErrorCode();
            NX_WARNING(this, lit("Failed to set timeout for HTTP connection from %1. %2").
                arg(d->socket->getForeignAddress().toString()).arg(SystemError::toString(osErrorCode)));
            return;
        }

        std::string connectionGuid;
        for( ;; )
        {
            if( !connectionGuid.empty() )
            {
                //waiting for connection to be ready to receive more transactions
                d->messageBus->waitForNewTransactionsReady( connectionGuid );
            }

            if( !readSingleRequest() )
            {
                if( d->prevSocketError == SystemError::timedOut )
                {
                    NX_DEBUG(this, lm("Keep-alive timeout on transaction connection %1 from peer %2")
                        .args(connectionGuid, d->socket->getForeignAddress().toString()));
                }
                break;
            }

            if( d->request.requestLine.method != nx::network::http::Method::post &&
                d->request.requestLine.method != nx::network::http::Method::put )
            {
                sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
                break;
            }

            auto connectionGuidIter = d->request.headers.find( Qn::EC2_CONNECTION_GUID_HEADER_NAME );
            if( connectionGuidIter == d->request.headers.end() )
            {
                sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
                break;
            }

            const std::string requestConnectionGuid = connectionGuidIter->second.toStdString();
            if( connectionGuid.empty() )
            {
                connectionGuid = requestConnectionGuid;
                d->messageBus->waitForNewTransactionsReady( connectionGuid ); // wait while transaction transport goes to the ReadyForStreamingState
            }
            else if( requestConnectionGuid != connectionGuid )
            {
                //not allowing to use TCP same connection for multiple transaction connections
                sendResponse( nx::network::http::StatusCode::forbidden, nx::network::http::StringType() );
                break;
            }

            if( !d->messageBus->gotTransactionFromRemotePeer(
                    connectionGuid,
                    d->request,
                    d->requestBody ) )
            {
                NX_WARNING(this, lm("Received transaction from %1 for unknown connection %2")
                    .args(d->socket->getForeignAddress().toString(), connectionGuid));
                sendResponse(
                    nx::network::http::StatusCode::notFound,
                    nx::network::http::StringType());
                break;
            }

            //checking whether connection persistent or not...
            bool closeConnection = true;
            if( d->request.requestLine.version == nx::network::http::http_1_1 )
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

            sendResponse( nx::network::http::StatusCode::ok, nx::network::http::StringType() );

            if( closeConnection )
                break;
        }

        if( !connectionGuid.empty() )
            d->messageBus->connectionFailure( connectionGuid );
    }
}
