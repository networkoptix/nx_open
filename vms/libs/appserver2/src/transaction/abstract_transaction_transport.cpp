#include "abstract_transaction_transport.h"

#include <nx/vms/api/types/connection_types.h>

using namespace nx::vms;

namespace ec2 {

static bool skipTransactionForMobileClient(ApiCommand::Value command)
{
    switch (command)
    {
        case ApiCommand::getMediaServersEx:
        case ApiCommand::saveCameras:
        case ApiCommand::getCamerasEx:
        case ApiCommand::getUsers:
        case ApiCommand::saveLayouts:
        case ApiCommand::getLayouts:
        case ApiCommand::removeResource:
        case ApiCommand::removeCamera:
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeUser:
        case ApiCommand::removeLayout:
        case ApiCommand::saveCamera:
        case ApiCommand::saveMediaServer:
        case ApiCommand::saveUser:
        case ApiCommand::saveUsers:
        case ApiCommand::saveLayout:
        case ApiCommand::setResourceStatus:
        case ApiCommand::setResourceParam:
        case ApiCommand::setResourceParams:
        case ApiCommand::saveCameraUserAttributes:
        case ApiCommand::saveMediaServerUserAttributes:
        case ApiCommand::getCameraHistoryItems:
        case ApiCommand::addCameraHistoryItem:
            return false;
        default:
            break;
    }
    return true;
}

bool QnAbstractTransactionTransport::shouldTransactionBeSentToPeer(const QnAbstractTransaction& transaction)
{
    if (remotePeer().peerType == api::PeerType::oldMobileClient && skipTransactionForMobileClient(transaction.command))
        return false;
    else if (remotePeer().peerType == api::PeerType::oldServer)
        return false;

    else if (transaction.isLocal() && !remotePeer().isClient())
        return false;

    if (remotePeer().peerType == api::PeerType::cloudServer)
    {
        if (transaction.transactionType != TransactionType::Cloud &&
            transaction.command != ApiCommand::tranSyncRequest &&
            transaction.command != ApiCommand::tranSyncResponse &&
            transaction.command != ApiCommand::tranSyncDone)
        {
            return false;
        }
        if (transaction.peerID == remotePeer().id)
            return false; //< Don't send cloud transactions back to the Cloud.
    }

    return true;
}

} // namespace ec2
