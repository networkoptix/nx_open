#pragma once

#include <nx/streaming/abstract_data_consumer.h>
#include <rest/server/request_handler.h>

#include <network/tcp_connection_processor.h>
#include <network/tcp_listener.h>

namespace ec2
{

    class QnHttpTransactionReceiverPrivate;
    class ServerTransactionMessageBus;

    /*!
        Using this HTTP handler because REST does not support HTTP interleaving
    */
    class QnHttpTransactionReceiver
    :
        public QnTCPConnectionProcessor
    {
    public:
        QnHttpTransactionReceiver(
            std::unique_ptr<nx::network::AbstractStreamSocket> socket,
            QnTcpListener* owner,
            ServerTransactionMessageBus* messageBus);
        virtual ~QnHttpTransactionReceiver();

    protected:
        virtual void run() override;

    private:
        Q_DECLARE_PRIVATE(QnHttpTransactionReceiver);
    };
}
