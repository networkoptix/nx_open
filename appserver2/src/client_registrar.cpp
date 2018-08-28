#include "client_registrar.h"

#include <nx/utils/log/log.h>

#include <api/runtime_info_manager.h>
#include <transaction/abstract_transaction_message_bus.h>
#include <transaction/abstract_transaction_transport.h>

namespace ec2 {

ClientRegistrar::ClientRegistrar(
    AbstractTransactionMessageBus* messageBus,
    QnRuntimeInfoManager* runtimeInfoManager)
    :
    m_messageBus(messageBus),
    m_runtimeInfoManager(runtimeInfoManager)
{
    Qn::directConnect(m_messageBus, &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &ClientRegistrar::onNewConnectionEstablished);
}

ClientRegistrar::~ClientRegistrar()
{
    directDisconnectAll();
}

void ClientRegistrar::onNewConnectionEstablished(
    QnAbstractTransactionTransport* transport)
{
    const auto remotePeer = transport->remotePeer();
    if (!remotePeer.isClient())
        return;

    QnPeerRuntimeInfo peerRuntimeInfo;
    peerRuntimeInfo.uuid = remotePeer.id;
    peerRuntimeInfo.data.peer = remotePeer;

    const auto queryParams = transport->httpQueryParams();
    if (auto videowallGuidIter = queryParams.find("videowallGuid");
        videowallGuidIter != queryParams.end())
    {
        peerRuntimeInfo.data.videoWallInstanceGuid = QnUuid(videowallGuidIter->second);
        peerRuntimeInfo.data.peer.peerType = Qn::PT_VideowallClient;
    }

    m_runtimeInfoManager->updateRemoteItem(peerRuntimeInfo);
}

} // namespace ec2
