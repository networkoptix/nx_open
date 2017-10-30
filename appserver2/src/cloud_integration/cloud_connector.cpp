#include "cloud_connector.h"

#include "../transaction/transaction_message_bus.h"

namespace ec2 {

CloudConnector::CloudConnector(AbstractTransactionMessageBus* messageBus):
    m_messageBus(messageBus)
{
}

void CloudConnector::startDataSynchronization(const nx::utils::Url &cloudUrl)
{
    m_messageBus->addOutgoingConnectionToPeer(::ec2::kCloudPeerId, cloudUrl);
}

void CloudConnector::stopDataSynchronization()
{
    m_messageBus->removeOutgoingConnectionFromPeer(::ec2::kCloudPeerId);
}

} // namespace ec2
