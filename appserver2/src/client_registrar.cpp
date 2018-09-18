#include "client_registrar.h"

#include <nx/utils/log/log.h>

#include <api/runtime_info_manager.h>
#include <transaction/abstract_transaction_message_bus.h>
#include <transaction/abstract_transaction_transport.h>
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>

using namespace nx::vms;

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

    loadQueryParams(&peerRuntimeInfo, transport->httpQueryParams());

    m_runtimeInfoManager->updateRemoteItem(peerRuntimeInfo);

    auto commonModule = m_messageBus->commonModule();
    ec2::QnTransaction<api::RuntimeData> tran(
        ec2::ApiCommand::runtimeInfoChanged, commonModule->moduleGUID());
    tran.params = peerRuntimeInfo.data;
    commonModule->ec2Connection()->messageBus()->sendTransaction(tran);
}

void ClientRegistrar::loadQueryParams(
    QnPeerRuntimeInfo* peerRuntimeInfo,
    const std::multimap<QString, QString>& queryParams)
{
    loadQueryParam(queryParams,
        "videoWallInstanceGuid",
        &peerRuntimeInfo->data.videoWallInstanceGuid);

    loadQueryParam(queryParams,
        "videoWallControlSession",
        &peerRuntimeInfo->data.videoWallControlSession);

    if (!peerRuntimeInfo->data.videoWallInstanceGuid.isNull() ||
        !peerRuntimeInfo->data.videoWallControlSession.isNull())
    {
        peerRuntimeInfo->data.peer.peerType = nx::vms::api::PeerType::videowallClient;
    }
}

bool ClientRegistrar::loadQueryParam(
    const std::multimap<QString, QString>& queryParams,
    const QString& name,
    QnUuid* value)
{
    auto it = queryParams.find(name);
    if (it == queryParams.end())
        return false;

    *value = QnUuid(it->second);
    return true;
}

} // namespace ec2
