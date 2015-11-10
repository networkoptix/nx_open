#include "mobile_client_message_processor.h"

#include "core/resource/resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "common/common_module.h"

QnMobileClientMessageProcessor::QnMobileClientMessageProcessor() :
    base_type()
{
}

bool QnMobileClientMessageProcessor::isConnected() const {
    switch (connectionState()) {
    case QnConnectionState::Connected:
    case QnConnectionState::Reconnecting:
    case QnConnectionState::Ready:
        return true;
    default:
        return false;
    }
}
