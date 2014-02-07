#include "transaction_message_bus.h"
#include "remote_ec_connection.h"

namespace ec2
{

template <class T>
void QnTransactionMessageBus::CustomHandler<T>::processByteArray(QByteArray data)
{
    QnTransaction<ApiCameraData> tran1;

    m_handler->processTransaction<ApiCameraData>(tran1);

    QnTransaction<ApiResourceData> tran2;
    m_handler->processTransaction<ApiResourceData>(tran2);
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;

}
