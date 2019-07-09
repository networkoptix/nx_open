#include "update_connection_speed_handler.h"

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/buffer_source.h>

#include "../listening_peer_db.h"
#include "http_api_path.h"

namespace nx::hpm {

using namespace nx::network::http;

namespace {

std::unique_ptr<AbstractMsgBodySource>makeMessageBody(QByteArray message)
{
    return std::make_unique<BufferSource>("text/plain", std::move(message));
}

} // namespace

UpdateConnectionSpeedHandler::UpdateConnectionSpeedHandler(ListeningPeerDb* listeningPeerDb):
    m_listeningPeerDb(listeningPeerDb)
{
}

void UpdateConnectionSpeedHandler::processRequest(
    RequestContext /*requestContext*/,
    api::PeerConnectionSpeed connectionSpeed)
{
    if (connectionSpeed.serverId.empty())
    {
        NX_WARNING(this, "Ignoring empty serverId");
        return requestCompleted(
            StatusCode::badRequest,
            makeMessageBody("Empty serverId"));
    }

    if (connectionSpeed.systemId.empty())
    {
        NX_WARNING(this, "Ignoring empty systemId");
        return requestCompleted(
            StatusCode::badRequest,
            makeMessageBody("Empty systemId"));
    }

    updateConnectionSpeed(connectionSpeed);
}

void UpdateConnectionSpeedHandler::updateConnectionSpeed(
    const api::PeerConnectionSpeed& connectionSpeed)
{
    std::string peerId = connectionSpeed.serverId + "." + connectionSpeed.systemId;

    m_listeningPeerDb->addUplinkSpeed(
        peerId,
        connectionSpeed.connectionSpeed,
        [this, peerId](bool success)
        {
            if (!success)
            {
                NX_WARNING(this, "Failed to update connection speed for peer: %1", peerId);
                return requestCompleted(
                    StatusCode::internalServerError,
                    makeMessageBody("Failed to update uplink connection speed"));
            }
            requestCompleted(StatusCode::ok);
        });
}

} // namespace nx::hpm
