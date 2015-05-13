/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "http_transaction_receiver.h"

#include <rest/server/rest_connection_processor.h>
#include <utils/network/tcp_connection_priv.h>

#include "http/custom_headers.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    QnHttpTransactionReceiver::QnHttpTransactionReceiver()
    {
    }

    int QnHttpTransactionReceiver::executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray&, /*contentType*/ 
        const QnRestConnectionProcessor* )
    {
        return nx_http::StatusCode::badRequest;
    }

    int QnHttpTransactionReceiver::executePost(
        const QString& path,
        const QnRequestParamList& /*params*/,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& resultBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* connection )
    {
        auto connectionGuidIter = connection->request().headers.find( nx_ec::EC2_CONNECTION_GUID_HEADER_NAME );
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

}
