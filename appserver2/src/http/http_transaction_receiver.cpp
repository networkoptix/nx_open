/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "http_transaction_receiver.h"

#include <utils/network/tcp_connection_priv.h>

#include "http/custom_headers.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
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

        if (d->clientRequest.isEmpty()) {
            if (!readRequest())
                return;
        }
        parseRequest();
        d->response.headers.emplace( "Connection", "close" );

        if( d->request.requestLine.method != nx_http::Method::POST )
        {
            sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
            return;
        }

        auto connectionGuidIter = d->request.headers.find( nx_ec::EC2_CONNECTION_GUID_HEADER_NAME );
        if( connectionGuidIter == d->request.headers.end() )
        {
            sendResponse( nx_http::StatusCode::forbidden, nx_http::StringType() );
            return;
        }
        const QnUuid connectionGuid( connectionGuidIter->second );

        if( !QnTransactionMessageBus::instance()->gotTransactionFromRemotePeer(
                connectionGuid,
                d->request,
                d->requestBody ) )
        {
            sendResponse( nx_http::StatusCode::notFound, nx_http::StringType() );
            return;
        }
    
        sendResponse( nx_http::StatusCode::ok, nx_http::StringType() );
        return;
    }
}
