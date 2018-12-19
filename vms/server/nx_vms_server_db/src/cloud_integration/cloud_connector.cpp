#include "cloud_connector.h"

#include <transaction/transaction_message_bus.h>

namespace ec2 {

CloudConnector::CloudConnector(AbstractTransactionMessageBus* messageBus):
    m_messageBus(messageBus)
{
}

void CloudConnector::startDataSynchronization(
    const std::string& peerId,
    const nx::utils::Url& cloudUrl)
{
    QnMutexLocker lock(&m_mutex);

    m_currentPeerId = QnUuid::fromStringSafe(peerId);

    m_messageBus->addOutgoingConnectionToPeer(
        *m_currentPeerId,
        nx::vms::api::PeerType::cloudServer,
        cloudUrl);
}

void CloudConnector::stopDataSynchronization()
{
    QnMutexLocker lock(&m_mutex);

    if (m_currentPeerId)
    {
        m_messageBus->removeOutgoingConnectionFromPeer(*m_currentPeerId);
        m_currentPeerId = std::nullopt;
    }
}

} // namespace ec2
