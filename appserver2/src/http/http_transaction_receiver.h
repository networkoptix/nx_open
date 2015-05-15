/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef HTTP_TRANSACTION_RECEIVER_H
#define HTTP_TRANSACTION_RECEIVER_H

#include <core/dataconsumer/abstract_data_consumer.h>
#include <rest/server/request_handler.h>
#include <utils/network/tcp_connection_processor.h>
#include <utils/network/tcp_listener.h>


namespace ec2
{
    class QnHttpTransactionReceiver
    :
        public QnRestRequestHandler
    {
    public:
        QnHttpTransactionReceiver();

        //!Implementation of QnRestRequestHandler::executeGet
        virtual int executeGet(
            const QString& /*path*/,
            const QnRequestParamList& /*params*/,
            QByteArray& /*result*/,
            QByteArray&, /*contentType*/ 
            const QnRestConnectionProcessor* ) override;

        //!Implementation of QnRestRequestHandler::executePost
        virtual int executePost(
            const QString& path,
            const QnRequestParamList& /*params*/,
            const QByteArray& body,
            const QByteArray& srcBodyContentType,
            QByteArray& resultBody,
            QByteArray& contentType,
            const QnRestConnectionProcessor* ) override;

    private:
        QnHttpTransactionReceiver( const QnHttpTransactionReceiver& );
        QnHttpTransactionReceiver& operator=( const QnHttpTransactionReceiver& );
    };
}

#endif  //HTTP_TRANSACTION_RECEIVER_H
