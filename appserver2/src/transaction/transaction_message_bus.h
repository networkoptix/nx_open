#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <nx_ec/ec_api.h>
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction.h"

namespace ec2
{
    class TransactionHandler
    {
        template <class T>
        void processTransaction(QnTransaction<ApiCameraData> tran)
        {

        }

        template <class T>
        void processTransaction(QnTransaction<ApiResourceData> tran)
        {

        }
    };

    class QnTransactionMessageBus
    {
    private:
        class AbstractHandler
        {
        public:
            virtual void processByteArray(QByteArray data) = 0;
        };

        template <class T>
        class CustomHandler: public AbstractHandler
        {
        public:
            CustomHandler(T* handler): m_handler(handler) {}

            virtual void processByteArray(QByteArray data) override;
        private:
            T* m_handler;
        };

    public:
        QnTransactionMessageBus();

        ErrorCode addConnectionToPeer(const QUrl& url);
        void removeConnectionFromPeer(const QUrl& url);

        template <class T>
        void setHandler(T* handler) { m_handler = new CustomHandler<T>(handler); }
    private:
        void gotTransaction(QByteArray data)
        {
            m_handler->processByteArray(data);
        }
    private:
        AbstractHandler* m_handler;
    };
}

#endif // __TRANSACTION_MESSAGE_BUS_H_
