/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef HTTP_TRANSACTION_RECEIVER_H
#define HTTP_TRANSACTION_RECEIVER_H

#include "utils/network/tcp_connection_processor.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/network/tcp_listener.h"


namespace ec2
{
    class QnHttpTransactionReceiverPrivate;

    class QnHttpTransactionReceiver
    :
        public QnTCPConnectionProcessor
    {
    public:
        QnHttpTransactionReceiver( QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner );
        virtual ~QnHttpTransactionReceiver();

    protected:
        virtual void run() override;

    private:
        Q_DECLARE_PRIVATE(QnHttpTransactionReceiver);
    };
}

#endif  //HTTP_TRANSACTION_RECEIVER_H
