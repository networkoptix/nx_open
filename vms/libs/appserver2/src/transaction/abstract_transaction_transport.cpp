// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_transaction_transport.h"

#include <nx/vms/api/types/connection_types.h>
#include <nx/p2p/transaction_filter.h>

using namespace nx::vms;

namespace ec2 {

bool QnAbstractTransactionTransport::skipTransactionForMobileClient(
    ApiCommand::Value command)
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

} // namespace ec2
