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
    if (!remotePeer.isClient() || remotePeer.id.isNull())
        return;

    QnPeerRuntimeInfo peerRuntimeInfo;
    if (m_runtimeInfoManager->hasItem(remotePeer.id))
        peerRuntimeInfo = m_runtimeInfoManager->item(remotePeer.id);

    peerRuntimeInfo.uuid = remotePeer.id;
    peerRuntimeInfo.data.peer = remotePeer;

    const auto queryParams = transport->httpQueryParams();
    if (auto videoWallInstanceGuidIter = queryParams.find("videoWallInstanceGuid");
        videoWallInstanceGuidIter != queryParams.end())
    {
        peerRuntimeInfo.data.videoWallInstanceGuid =
            QnUuid(videoWallInstanceGuidIter->second);
        peerRuntimeInfo.data.peer.peerType = nx::vms::api::PeerType::videowallClient;
    }

    m_runtimeInfoManager->updateRemoteItem(peerRuntimeInfo);
}

} // namespace ec2
